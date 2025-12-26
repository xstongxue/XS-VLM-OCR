#include "GeneralAdapter.h"
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

GeneralAdapter::GeneralAdapter(const ModelConfig& config, QObject* parent)
    : ModelAdapter(config, parent),
      m_initialized(false),
      m_temperature(0.1f)
{
    m_modelName = m_config.params.value("model_name", "Qwen/Qwen3-VL-8B-Instruct");
    m_apiKey = m_config.params.value("api_key", "");
    QString host = m_config.params.value("api_host", "https://api.siliconflow.cn");
    if (host.endsWith("/"))
        m_apiUrl = host + "v1/chat/completions";
    else
        m_apiUrl = host + "/v1/chat/completions";

    if (m_config.params.contains("temperature")) {
        bool ok;
        float t = m_config.params["temperature"].toFloat(&ok);
        if (ok && t >= 0.0f && t <= 1.0f) m_temperature = t;
    }
}

GeneralAdapter::~GeneralAdapter() = default;

bool GeneralAdapter::initialize()
{
    QMutexLocker locker(&m_mutex);
    if (m_config.type == "online" && m_apiKey.isEmpty()) {
        qWarning() << "GeneralAdapter: 缺少 api_key";
        return false;
    }
    if (m_apiUrl.isEmpty()) {
        qWarning() << "GeneralAdapter: 缺少 api_host/api_url";
        return false;
    }
    m_initialized = true;
    qDebug() << "GeneralAdapter init ok" << m_modelName << m_apiUrl;
    return true;
}

QString GeneralAdapter::encodeImageToBase64(const QImage& image)
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

QString GeneralAdapter::parseAPIResponse(const QByteArray& response, QString& errorMsg)
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
    QJsonArray choices = obj.value("choices").toArray();
    if (choices.isEmpty()) {
        errorMsg = "choices 为空";
        return {};
    }
    QJsonObject msgObj = choices[0].toObject().value("message").toObject();
    QJsonValue contentVal = msgObj.value("content");
    if (contentVal.isString())
        return contentVal.toString();
    if (contentVal.isArray()) {
        QStringList parts;
        for (const QJsonValue& v : contentVal.toArray()) {
            if (v.isObject()) {
                QJsonObject o = v.toObject();
                if (o.value("type").toString() == "text")
                    parts << o.value("text").toString();
            }
        }
        return parts.join("\n");
    }
    errorMsg = "未找到 content";
    return {};
}

QString GeneralAdapter::callAPI(const QString& imageBase64, const QString& prompt, bool hasImage, QString& errorMsg)
{
    QNetworkAccessManager manager;
    QNetworkRequest request{QUrl(m_apiUrl)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!m_apiKey.isEmpty())
        request.setRawHeader("Authorization", QString("Bearer %1").arg(m_apiKey).toUtf8());

    QJsonArray contentArray;
    if (!prompt.isEmpty())
        contentArray.append(QJsonObject{{"type", "text"}, {"text", prompt}});
    if (hasImage) {
        QString dataUrl = QString("data:image/png;base64,%1").arg(imageBase64);
        contentArray.append(QJsonObject{
            {"type", "image_url"},
            {"image_url", QJsonObject{{"url", dataUrl}}}
        });
    }

    QJsonObject body{
        {"model", m_modelName},
        {"temperature", m_temperature},
        {"messages", QJsonArray{
            QJsonObject{
                {"role", "user"},
                {"content", contentArray}
            }
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

OCRResult GeneralAdapter::recognize(const QImage& image, const QString& prompt)
{
    OCRResult result;
    QElapsedTimer timer;
    timer.start();

    if (!m_initialized) {
        result.success = false;
        result.errorMessage = "GeneralAdapter 未初始化";
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