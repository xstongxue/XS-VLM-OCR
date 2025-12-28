#include "GLMAdapter.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QBuffer>
#include <QDebug>
#include <QElapsedTimer>
#include <QUrlQuery>
#include <QTimer>
#include <QEventLoop>
#include <QThread>
#include <QMutexLocker>
#include <QRegularExpression>

GLMAdapter::GLMAdapter(const ModelConfig &config, QObject *parent)
    : ModelAdapter(config, parent), m_initialized(false), m_networkManager(nullptr), 
      m_temperature(0.7f), m_enableThinking(true)
{
    // 从配置中读取参数
    m_modelName = m_config.params.value("model_name", "glm-4.5v");
    
    // 验证模型名称
    if (m_modelName != "glm-4.6v" && m_modelName != "glm-4.5v") {
        qWarning() << "GLMAdapter: 不支持的模型名称，使用默认值 glm-4.5v";
        m_modelName = "glm-4.5v";
    }
    
    m_apiKey = m_config.params.value("api_key", "");
    
    // 默认 API 地址
    QString defaultHost = "https://open.bigmodel.cn/api/paas/v4/chat/completions";
    QString host = m_config.params.value("api_host", defaultHost);
    
    // 检查 host 是否已经包含完整路径
    if (host.contains("/chat/completions")) {
        m_apiUrl = host;
    } else {
        // 确保 host 以 / 结尾
        if (!host.endsWith("/")) {
            m_apiUrl = host + "/api/paas/v4/chat/completions";
        } else {
            m_apiUrl = host + "api/paas/v4/chat/completions";
        }
    }

    // 温度参数[GLM的API是不支持的，所以这里只接收不使用]
    if (m_config.params.contains("temperature")) {
        bool ok;
        float temp = m_config.params["temperature"].toFloat(&ok);
        if (ok && temp >= 0.0f && temp <= 1.0f) {
            m_temperature = temp;
        }
    }
    
    // 思考过程参数
    if (m_config.params.contains("enable_thinking")) {
        m_enableThinking = m_config.params["enable_thinking"].toLower() == "true" || 
                          m_config.params["enable_thinking"] == "1";
    }
}

GLMAdapter::~GLMAdapter()
{
    if (m_networkManager) {
        delete m_networkManager;
    }
}

bool GLMAdapter::initialize()
{
    QMutexLocker locker(&m_mutex);

    qDebug() << "=== Initializing GLMAdapter ===";
    qDebug() << "Model:" << m_modelName;
    qDebug() << "Type:" << m_config.type;
    qDebug() << "API URL:" << m_apiUrl;
    qDebug() << "API Key:" << (m_apiKey.isEmpty() ? "NOT SET" : QString("***") + m_apiKey.right(4));

    // 检查必要的配置
    if (m_config.type == "online" && m_apiKey.isEmpty()) {
        qWarning() << "GLMAdapter: 在线模型需要 API key";
        qWarning() << "请在配置中设置 api_key 参数";
        return false;
    }

    if (m_apiUrl.isEmpty()) {
        qWarning() << "GLMAdapter: API URL 为空";
        return false;
    }

    m_initialized = true;
    qDebug() << "GLMAdapter: 初始化成功！";
    return true;
}

QString GLMAdapter::encodeImageToBase64(const QImage &image)
{
    if (image.isNull()) {
        return QString();
    }

    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    
    // 转换为 PNG 格式
    image.save(&buffer, "PNG");
    
    return byteArray.toBase64();
}

OCRResult GLMAdapter::recognize(const QImage &image, const QString &prompt)
{
    QMutexLocker locker(&m_mutex);
    QElapsedTimer timer;
    timer.start();

    qDebug() << "";
    qDebug() << "=== 开始 GLM OCR 识别 ===";

    OCRResult result;
    result.modelName = m_config.displayName;
    
    if (!m_initialized) {
        result.success = false;
        result.errorMessage = "GLM 模型未初始化";
        qWarning() << "GLMAdapter: 错误: GLM 模型未初始化";
        return result;
    }

    // 判断是否有有效图片
    bool hasValidImage = !image.isNull() && image.width() > 0 && image.height() > 0;
    
    // 编码图片
    QString imageBase64;
    if (hasValidImage) {
        imageBase64 = encodeImageToBase64(image);
        if (imageBase64.isEmpty()) {
            result.success = false;
            result.errorMessage = "图片编码失败";
            qWarning() << "GLMAdapter: 错误: 图片编码失败";
            return result;
        }
    }

    QString errorMsg;
    QString text = callAPI(imageBase64, prompt, hasValidImage, errorMsg);

    if (text.isEmpty() && !errorMsg.isEmpty()) {
        result.success = false;
        result.errorMessage = errorMsg;
        qWarning() << "GLMAdapter: 错误:" << errorMsg;
        return result;
    }

    result.fullText = text;
    result.success = true;
    result.processingTimeMs = timer.elapsed();
    
    qDebug() << "GLMAdapter: 识别成功，文本长度:" << text.length() << "字符，耗时:" << result.processingTimeMs << "ms";
    
    return result;
}

QString GLMAdapter::callAPI(const QString& imageBase64, const QString& prompt, bool hasImage, QString& errorMsg)
{
    // 在工作线程中创建网络管理器
    QNetworkAccessManager networkManager;
    
    qDebug() << "";
    qDebug() << "=== GLMAdapter: 调用 GLM API ===";
    qDebug() << "GLMAdapter: 模型:" << m_modelName;
    qDebug() << "GLMAdapter: 思考过程:" << (m_enableThinking ? "启用" : "禁用");

    // 构建请求
    QNetworkRequest request;
    QUrl url(m_apiUrl);
    request.setUrl(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    // 设置连接属性
    request.setRawHeader("Connection", "keep-alive");
    request.setRawHeader("Accept", "application/json");

    // 设置认证头（Bearer token）
    if (!m_apiKey.isEmpty()) {
        QString authHeader = QString("Bearer %1").arg(m_apiKey);
        request.setRawHeader("Authorization", authHeader.toUtf8());
        qDebug() << "GLMAdapter: 认证: Bearer ***" << m_apiKey.right(4);
    } else {
        qDebug() << "GLMAdapter: 未设置 API Key";
    }

    // 构建请求体（GLM API 格式）
    QJsonObject payload;
    payload["model"] = m_modelName;

    // 构建消息内容
    QJsonArray messages;
    QJsonObject message;
    message["role"] = "user";
    
    // content 数组（GLM 支持多个 image_url 和 text）
    QJsonArray content;

    // 如果有图片，先添加图片
    if (hasImage && !imageBase64.isEmpty()) {
        QJsonObject imagePart;
        imagePart["type"] = "image_url";
        QJsonObject imageUrl;
        imageUrl["url"] = "data:image/png;base64," + imageBase64;
        imagePart["image_url"] = imageUrl;
        content.append(imagePart);
    }

    // 添加文本提示（如果有图片，文本在图片后面）
    QString finalPrompt = prompt.isEmpty() ? 
        (hasImage ? "请识别图片中的所有文字内容，不输出任何解释说明，仅输出识别文本。" : "请回答以下问题。") 
        : prompt;
    QJsonObject textPart;
    textPart["type"] = "text";
    textPart["text"] = finalPrompt;
    content.append(textPart);

    message["content"] = content;
    messages.append(message);
    payload["messages"] = messages;
    
    // 注意：GLM API 不支持 temperature 参数，移除
    // payload["temperature"] = m_temperature;  // GLM API 不支持此参数
    
    // 添加思考过程参数（如果启用）
    if (m_enableThinking) {
        QJsonObject thinking;
        thinking["type"] = "enabled";
        payload["thinking"] = thinking;
    }
    
    // 可选参数：max_tokens
    if (m_config.params.contains("max_tokens")) {
        bool ok;
        int maxTokens = m_config.params["max_tokens"].toInt(&ok);
        if (ok && maxTokens > 0) {
            payload["max_tokens"] = maxTokens;
        }
    }

    // 转换为 JSON 格式
    QJsonDocument requestDoc(payload);
    QByteArray requestData = requestDoc.toJson(QJsonDocument::Compact);

    qDebug() << "GLMAdapter: 提示词:" << finalPrompt;

    // 打印请求体（调试用，限制长度）
    if (requestData.size() < 1000) {
        qDebug() << "GLMAdapter: 完整请求体:" << QString::fromUtf8(requestData);
    } else {
        QString preview = QString::fromUtf8(requestData.left(300)) +
                          "\n... [省略 " + QString::number(requestData.size() - 600) + " 字节] ...\n" +
                          QString::fromUtf8(requestData.right(300));
        qDebug() << "GLMAdapter: 请求体预览:" << preview;
    }

    qDebug() << "";
    qDebug() << "GLMAdapter: 发送 HTTP POST 请求...";

    // 发送请求
    QNetworkReply *reply = networkManager.post(request, requestData);

    // 等待响应（同步方式）
    QEventLoop loop;
    QByteArray responseData;
    
    // 在readyRead时累积读取数据（处理流式响应）
    bool hasReceivedData = false;
    connect(reply, &QNetworkReply::readyRead, [reply, &responseData, &hasReceivedData]() {
        hasReceivedData = true;
        QByteArray newData = reply->readAll();
        responseData.append(newData);
    });
    
    // 连接多个信号，确保能捕获所有情况
    connect(reply, &QNetworkReply::finished, [&loop, &responseData, reply, &hasReceivedData]() {
        // 确保读取所有剩余数据
        if (reply->bytesAvailable() > 0) {
            QByteArray remaining = reply->readAll();
            responseData.append(remaining);
            hasReceivedData = true;
        }
        loop.quit();
    });
    
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::error),
            [](QNetworkReply::NetworkError error) {
                qDebug() << "GLMAdapter: 收到错误信号:" << error;
            });

    // 设置超时（60秒）
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, [&loop, reply]() {
        qWarning() << "GLMAdapter: 请求超时！";
        loop.quit();
    });
    timer.start(60000);

    qDebug() << "GLMAdapter: 等待响应... (超时: 60秒)";

    QElapsedTimer requestTimer;
    requestTimer.start();

    loop.exec();

    qint64 requestTime = requestTimer.elapsed();
    qDebug() << "GLMAdapter: 请求耗时:" << requestTime << "ms";

    // 检查是否超时
    if (!reply->isFinished()) {
        reply->abort();
        reply->deleteLater();
        qWarning() << "GLMAdapter: 请求超时！";
        errorMsg = "请求超时";
        return QString();
    }
    
    // 再次确保读取所有数据（双重保险）
    if (reply->bytesAvailable() > 0) {
        QByteArray remaining = reply->readAll();
        responseData.append(remaining);
        hasReceivedData = true;
    }
    
    // 检查 HTTP 状态码
    QVariant statusCodeVariant = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    int statusCode = statusCodeVariant.isValid() ? statusCodeVariant.toInt() : 0;
    
    // 检查网络错误
    QNetworkReply::NetworkError networkError = reply->error();
    bool hasNetworkError = (networkError != QNetworkReply::NoError);
    
    qDebug() << "GLMAdapter: 收到响应";
    qDebug() << "GLMAdapter: HTTP 状态码:" << (statusCode > 0 ? QString::number(statusCode) : "未获取到");
    
    // 如果有响应数据，打印前500字符用于调试
    if (!responseData.isEmpty()) {
        QString responsePreview = QString::fromUtf8(responseData);
        if (responsePreview.length() > 500) {
            responsePreview = responsePreview.left(500) + "... [省略 " + QString::number(responsePreview.length() - 500) + " 字符]";
        }
        qDebug() << "GLMAdapter: 响应内容预览:" << responsePreview;
    }

    reply->deleteLater();

    // 检查 HTTP 状态码
    if (statusCode > 0 && statusCode != 200) {
        errorMsg = QString("HTTP 错误 %1").arg(statusCode);
        qWarning() << "GLMAdapter: HTTP 错误:" << statusCode;
        if (!responseData.isEmpty()) {
            // 尝试解析错误响应
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(responseData, &parseError);
            if (!parseError.error && doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.contains("error")) {
                    QJsonObject errorObj = obj["error"].toObject();
                    QString errorMsgFromServer = errorObj.value("message").toString();
                    if (!errorMsgFromServer.isEmpty()) {
                        errorMsg = QString("HTTP %1: %2").arg(statusCode).arg(errorMsgFromServer);
                    }
                }
            }
            qWarning() << "GLMAdapter: 响应内容:" << QString::fromUtf8(responseData);
        }
        return QString();
    }

    if (hasNetworkError && responseData.isEmpty()) {
        errorMsg = QString("网络错误: %1").arg(reply->errorString());
        qWarning() << "GLMAdapter: 网络错误:" << errorMsg;
        return QString();
    }

    qDebug() << "";
    qDebug() << "GLMAdapter: 解析 API 响应...";

    // 解析响应
    QString result = parseAPIResponse(responseData, errorMsg);

    if (!result.isEmpty()) {
        qDebug() << "GLMAdapter: 解析成功，提取到文本:" << result.length() << "字符";
    } else if (!errorMsg.isEmpty()) {
        qWarning() << "GLMAdapter: 解析失败:" << errorMsg;
    }

    return result;
}

QString GLMAdapter::parseAPIResponse(const QByteArray& response, QString& errorMsg)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(response, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        errorMsg = QString("JSON 解析错误: %1").arg(parseError.errorString());
        qWarning() << "GLMAdapter: JSON 解析失败:" << errorMsg;
        return QString();
    }
    
    if (!doc.isObject()) {
        errorMsg = "响应不是有效的 JSON 对象";
        qWarning() << "GLMAdapter: 响应不是 JSON 对象";
        return QString();
    }

    QJsonObject obj = doc.object();
    
    // GLM API 响应格式
    // {
    //   "id": "...",
    //   "model": "...",
    //   "choices": [
    //     {
    //       "index": 0,
    //       "message": {
    //         "role": "assistant",
    //         "content": "..."
    //       },
    //       "finish_reason": "stop"
    //     }
    //   ],
    //   "usage": {...}
    // }
    
    if (!obj.contains("choices")) {
        errorMsg = "响应中缺少 choices 字段";
        qWarning() << "GLMAdapter: 响应中缺少 choices 字段";
        if (obj.contains("error")) {
            QJsonObject errorObj = obj["error"].toObject();
            QString errorMessage = errorObj.value("message").toString();
            if (!errorMessage.isEmpty()) {
                errorMsg = errorMessage;
            }
        }
        return QString();
    }

    QJsonArray choices = obj["choices"].toArray();
    if (choices.isEmpty()) {
        errorMsg = "choices 数组为空";
        qWarning() << "GLMAdapter: choices 数组为空";
        return QString();
    }

    QJsonObject firstChoice = choices[0].toObject();
    if (!firstChoice.contains("message")) {
        errorMsg = "choice 中缺少 message 字段";
        qWarning() << "GLMAdapter: choice 中缺少 message 字段";
        return QString();
    }

    QJsonObject message = firstChoice["message"].toObject();
    QString content = message["content"].toString();

    if (content.isEmpty()) {
        errorMsg = "message.content 为空";
        qWarning() << "GLMAdapter: message.content 为空";
        return QString();
    }

    // 移除 GLM API 返回的边界标记 <|begin_of_box|> 和 <|end_of_box|>
    content.remove("<|begin_of_box|>");
    content.remove("<|end_of_box|>");
    
    // 移除可能存在的 ": " 前缀（在边界标记后的）
    content = content.trimmed();
    if (content.startsWith(": ")) {
        content = content.mid(2);
    } else if (content.startsWith(":")) {
        content = content.mid(1);
    }
    
    // 移除可能的引号包围
    content = content.trimmed();
    if (content.startsWith("\"") && content.endsWith("\"")) {
        content = content.mid(1, content.length() - 2);
    }
    
    // 移除末尾可能的逗号
    if (content.endsWith(",")) {
        content = content.left(content.length() - 1);
    }
    
    // 最终 trim
    content = content.trimmed();

    return content;
}