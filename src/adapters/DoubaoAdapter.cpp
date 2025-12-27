#include "DoubaoAdapter.h"
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
#include <QMutexLocker>

DoubaoAdapter::DoubaoAdapter(const ModelConfig& config, QObject* parent)
    : ModelAdapter(config, parent),
      m_initialized(false),
      m_temperature(0.1f)
{
    m_modelName = m_config.params.value("model_name", "doubao-seed-1-8-251215");
    m_apiKey = m_config.params.value("api_key", "");
    QString host = m_config.params.value("api_host", "https://ark.cn-beijing.volces.com");
    QString apiUrl = m_config.params.value("api_url", "");
    if (apiUrl.isEmpty()) {
        if (host.endsWith("/"))
            m_apiUrl = host + "api/v3/responses";
        else
            m_apiUrl = host + "/api/v3/responses";
    } else {
        m_apiUrl = apiUrl;
    }

    if (m_config.params.contains("temperature")) {
        bool ok;
        float t = m_config.params["temperature"].toFloat(&ok);
        if (ok && t >= 0.0f && t <= 1.0f) m_temperature = t;
    }
}

bool DoubaoAdapter::initialize()
{
    QMutexLocker locker(&m_mutex);
    if (m_initialized)
        return true;

    if (m_apiKey.isEmpty()) {
        qWarning() << "DoubaoAdapter: 缺少 api_key";
        return false;
    }
    if (m_apiUrl.isEmpty()) {
        qWarning() << "DoubaoAdapter: 缺少 api_url/api_host";
        return false;
    }

    m_initialized = true;
    qDebug() << "DoubaoAdapter init ok" << m_modelName << m_apiUrl;
    return true;
}

QString DoubaoAdapter::encodeImageToBase64(const QImage& image) const
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

QString DoubaoAdapter::parseResponse(const QByteArray& response, QString& errorMsg) const
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
        QJsonObject err = obj.value("error").toObject();
        QString msg = err.value("message").toString();
        if (msg.isEmpty())
            msg = err.value("code").toString("未知错误");
        errorMsg = msg;
        return {};
    }

    QJsonObject outputObj = obj.value("output").toObject();
    if (outputObj.contains("text") && outputObj.value("text").isString())
        return outputObj.value("text").toString();

    if (outputObj.contains("choices")) {
        QJsonArray choices = outputObj.value("choices").toArray();
        if (!choices.isEmpty()) {
            QJsonObject msgObj = choices[0].toObject().value("message").toObject();
            QJsonArray contentArr = msgObj.value("content").toArray();
            QStringList parts;
            for (const QJsonValue& v : contentArr) {
                if (v.isObject()) {
                    QJsonObject o = v.toObject();
                    QString type = o.value("type").toString();
                    if (type == "output_text" || type == "text")
                        parts << o.value("text").toString();
                }
            }
            if (!parts.isEmpty())
                return parts.join("\n");
        }
    }

    if (obj.contains("output_text") && obj.value("output_text").isString())
        return obj.value("output_text").toString();

    errorMsg = "未找到有效输出";
    return {};
}

QString DoubaoAdapter::callAPI(const QString& imageBase64, const QString& prompt, bool hasImage, QString& errorMsg)
{
    QNetworkAccessManager manager;
    QNetworkRequest request{QUrl(m_apiUrl)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!m_apiKey.isEmpty())
        request.setRawHeader("Authorization", QString("Bearer %1").arg(m_apiKey).toUtf8());

    QJsonArray contentArray;
    if (hasImage) {
        QString dataUrl = QString("data:image/png;base64,%1").arg(imageBase64);
        contentArray.append(QJsonObject{
            {"type", "input_image"},
            {"image_url", dataUrl}
        });
    }
    if (!prompt.isEmpty()) {
        contentArray.append(QJsonObject{
            {"type", "input_text"},
            {"text", prompt}
        });
    }

    QJsonObject body{
        {"model", m_modelName},
        {"input", QJsonArray{
            QJsonObject{
                {"role", "user"},
                {"content", contentArray}
            }
        }},
        {"parameters", QJsonObject{
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

    return parseResponse(respData, errorMsg);
}

OCRResult DoubaoAdapter::recognize(const QImage& image, const QString& prompt)
{
    QMutexLocker locker(&m_mutex);
    QElapsedTimer timer;
    timer.start();

    OCRResult result;
    result.modelName = m_config.displayName;

    if (!m_initialized) {
        result.success = false;
        result.errorMessage = "DoubaoAdapter 未初始化";
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