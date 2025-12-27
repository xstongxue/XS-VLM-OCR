#include "GeminiAdapter.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QBuffer>
#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <QUrl>

GeminiAdapter::GeminiAdapter(const ModelConfig& config, QObject* parent)
    : ModelAdapter(config, parent),
      m_initialized(false),
      m_temperature(0.1f)
{
    m_modelName = m_config.params.value("model_name", "gemini-2.5-flash");
    m_apiKey = m_config.params.value("api_key", "");
    m_apiHost = m_config.params.value("api_host", "https://generativelanguage.googleapis.com");
    if (m_config.params.contains("temperature")) {
        bool ok = false;
        float t = m_config.params["temperature"].toFloat(&ok);
        if (ok && t >= 0.0f && t <= 1.0f) m_temperature = t;
    }
}

GeminiAdapter::~GeminiAdapter() = default;

bool GeminiAdapter::initialize()
{
    QMutexLocker locker(&m_mutex);
    if (m_config.type == "online" && m_apiKey.isEmpty()) {
        qWarning() << "GeminiAdapter: 缺少 api_key";
        return false;
    }
    if (m_apiHost.isEmpty()) {
        qWarning() << "GeminiAdapter: 缺少 api_host";
        return false;
    }
    m_initialized = true;
    qDebug() << "GeminiAdapter init ok" << m_modelName << m_apiHost;
    return true;
}

QString GeminiAdapter::encodeImageToBase64(const QImage& image)
{
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    if (image.width() * image.height() > 1920 * 1080)
        image.save(&buf, "JPEG", 85);
    else
        image.save(&buf, "PNG");
    return bytes.toBase64();
}

QString GeminiAdapter::parseAPIResponse(const QByteArray& response, QString& errorMsg)
{
    QJsonParseError perr;
    QJsonDocument doc = QJsonDocument::fromJson(response, &perr);
    if (perr.error != QJsonParseError::NoError) {
        errorMsg = "JSON 解析错误: " + perr.errorString();
        return {};
    }
    if (!doc.isObject()) {
        errorMsg = "响应不是对象";
        return {};
    }
    QJsonObject obj = doc.object();
    if (obj.contains("error")) {
        errorMsg = obj["error"].toObject().value("message").toString("未知错误");
        return {};
    }
    QJsonArray cands = obj.value("candidates").toArray();
    if (cands.isEmpty()) {
        errorMsg = "candidates 为空";
        return {};
    }
    QJsonArray parts = cands[0].toObject().value("content").toObject().value("parts").toArray();
    QStringList texts;
    for (const QJsonValue& v : parts) {
        if (v.isObject()) {
            QString t = v.toObject().value("text").toString();
            if (!t.isEmpty()) texts << t;
        }
    }
    if (texts.isEmpty()) {
        errorMsg = "未找到 text 部分";
        return {};
    }
    return texts.join("\n");
}

QString GeminiAdapter::callAPI(const QString& imageBase64, const QString& prompt, bool hasImage, QString& errorMsg)
{
    QNetworkAccessManager manager;
    QString endpoint = QString("%1/v1beta/models/%2:generateContent").arg(m_apiHost, m_modelName);
    QNetworkRequest request{QUrl(endpoint)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!m_apiKey.isEmpty())
        request.setRawHeader("x-goog-api-key", m_apiKey.toUtf8());

    QJsonArray parts;
    if (hasImage) {
        parts.append(QJsonObject{
            {"inline_data", QJsonObject{
                {"mime_type", "image/png"},
                {"data", imageBase64}
            }}
        });
    }
    if (!prompt.isEmpty()) {
        parts.append(QJsonObject{{"text", prompt}});
    }

    QJsonObject body{
        {"contents", QJsonArray{
            QJsonObject{
                {"parts", parts}
            }
        }},
        {"safetySettings", QJsonArray{}},
        {"generationConfig", QJsonObject{
            {"temperature", m_temperature}
        }}
    };

    QByteArray payload = QJsonDocument(body).toJson();

    QEventLoop loop;
    QNetworkReply* reply = manager.post(request, payload);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(60000, &loop, &QEventLoop::quit);
    loop.exec();

    if (!reply->isFinished()) {
        errorMsg = "请求超时";
        reply->abort();
        reply->deleteLater();
        return {};
    }

    QByteArray respData = reply->readAll();
    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status < 200 || status >= 300) {
        errorMsg = QString("HTTP %1: %2").arg(status).arg(QString::fromUtf8(respData));
        reply->deleteLater();
        return {};
    }
    reply->deleteLater();

    return parseAPIResponse(respData, errorMsg);
}

OCRResult GeminiAdapter::recognize(const QImage& image, const QString& prompt)
{
    OCRResult result;
    QElapsedTimer timer;
    timer.start();

    if (!m_initialized) {
        result.success = false;
        result.errorMessage = "GeminiAdapter 未初始化";
        return result;
    }

    bool hasValidImage = !image.isNull() && image.width() > 0 && image.height() > 0;
    QString imageBase64;
    if (hasValidImage) {
        imageBase64 = encodeImageToBase64(image);
    }

    QString errorMsg;
    QString content = callAPI(imageBase64, prompt, hasValidImage, errorMsg);
    if (content.isEmpty()) {
        result.success = false;
        result.errorMessage = errorMsg.isEmpty() ? "空响应" : errorMsg;
    } else {
        result.success = true;
        result.fullText = content;
    }

    result.processingTimeMs = timer.elapsed();
    return result;
}
