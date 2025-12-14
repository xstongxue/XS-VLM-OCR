#include "CustomAdapter.h"
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

CustomAdapter::CustomAdapter(const ModelConfig &config, QObject *parent)
    : ModelAdapter(config, parent), m_initialized(false), m_networkManager(nullptr), m_temperature(0.1f)
{
    // 从配置中读取参数
    m_modelName = m_config.params.value("model_name", "gpt-4-vision-preview");
    m_apiKey = m_config.params.value("api_key", "");
    
    // 默认 API 地址（OpenAI 兼容格式）
    QString defaultHost = "https://api.openai.com/v1/chat/completions";
    QString host = m_config.params.value("api_host", defaultHost);
    
    // 检查 host 是否已经包含完整路径
    if (host.contains("/v1/chat/completions") || host.contains("/chat/completions"))
    {
        m_apiUrl = host;
    }
    else
    {
        // 确保 host 以 / 结尾
        if (!host.endsWith("/"))
        {
            m_apiUrl = host + "/v1/chat/completions";
        }
        else
        {
            m_apiUrl = host + "v1/chat/completions";
        }
    }

    // 温度参数
    if (m_config.params.contains("temperature"))
    {
        bool ok;
        float temp = m_config.params["temperature"].toFloat(&ok);
        if (ok && temp >= 0.0f && temp <= 1.0f)
        {
            m_temperature = temp;
        }
    }
    
    // Content-Type 设置（默认支持图片URL格式）
    m_contentType = m_config.params.value("content_type", "image_url");
}

CustomAdapter::~CustomAdapter()
{
    if (m_networkManager)
    {
        delete m_networkManager;
    }
}

bool CustomAdapter::initialize()
{
    QMutexLocker locker(&m_mutex);

    qDebug() << "=== Initializing CustomAdapter ===";
    qDebug() << "Model:" << m_modelName;
    qDebug() << "Type:" << m_config.type;
    qDebug() << "API URL:" << m_apiUrl;
    qDebug() << "API Key:" << (m_apiKey.isEmpty() ? "NOT SET" : QString("sk-***") + m_apiKey.right(4));

    // 检查必要的配置
    // 对于 online 类型的模型，API Key 是必需的
    // 对于 local 类型的模型，API Key 可以为空（本地服务不需要认证）
    if (m_config.type == "online" && m_apiKey.isEmpty())
    {
        qWarning() << "CustomAdapter: 在线模型需要 API key";
        qWarning() << "请在配置中设置 api_key 参数";
        return false;
    }

    if (m_apiUrl.isEmpty())
    {
        qWarning() << "CustomAdapter: API URL 为空";
        return false;
    }

    // 注意：不在这里创建 QNetworkAccessManager
    // 它会在 recognize() 方法中的工作线程里创建
    m_initialized = true;
    qDebug() << "CustomAdapter: 初始化成功！";
    return true;
}

QString CustomAdapter::encodeImageToBase64(const QImage &image)
{
    QByteArray imageData;
    QBuffer buffer(&imageData);
    buffer.open(QIODevice::WriteOnly);
    
    // 转换为 PNG 格式（兼容性最好）
    QImage convertedImage = image;
    if (convertedImage.format() != QImage::Format_RGB32 && convertedImage.format() != QImage::Format_ARGB32)
    {
        convertedImage = convertedImage.convertToFormat(QImage::Format_RGB32);
    }
    
    convertedImage.save(&buffer, "PNG");
    return imageData.toBase64();
}

QString CustomAdapter::callAPI(const QString& imageBase64, const QString& prompt, bool hasImage, QString& errorMsg)
{
    // 在工作线程中创建 QNetworkAccessManager
    QNetworkAccessManager networkManager;
    
    qDebug() << "";
    qDebug() << "=== CustomAdapter: 调用 OpenAI 兼容 API ===";
    qDebug() << "CustomAdapter: 模型:" << m_modelName;
    qDebug() << "CustomAdapter: 温度:" << m_temperature;

    // 构建请求
    QNetworkRequest request;
    QUrl url(m_apiUrl);
    request.setUrl(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    // 设置连接属性
    request.setRawHeader("Connection", "keep-alive");
    request.setRawHeader("Accept", "application/json");

    // 设置认证头（仅在 API Key 不为空时）
    if (!m_apiKey.isEmpty())
    {
        QString authHeader = QString("Bearer %1").arg(m_apiKey);
        request.setRawHeader("Authorization", authHeader.toUtf8());
        qDebug() << "CustomAdapter: 认证: Bearer sk-***" << m_apiKey.right(4);
    }
    else
    {
        qDebug() << "CustomAdapter: 未设置 API Key（本地部署可能不需要认证）";
    }

    // 构建请求体（OpenAI 兼容格式）
    QJsonObject payload;
    payload["model"] = m_modelName;

    // 构建消息内容
    QJsonArray messages;
    QJsonObject message;
    message["role"] = "user";

    // content 数组
    QJsonArray content;

    // 添加文本提示
    QString finalPrompt = prompt.isEmpty() ? 
        (hasImage ? "请识别图片中的所有文字内容，不输出任何解释说明，仅输出识别文本。" : "请回答以下问题。") 
        : prompt;
    QJsonObject textPart;
    textPart["type"] = "text";
    textPart["text"] = finalPrompt;
    content.append(textPart);

    // 只在有图片时添加图片部分
    if (hasImage && !imageBase64.isEmpty()) {
        QJsonObject imagePart;
        imagePart["type"] = "image_url";
        QJsonObject imageUrl;
        imageUrl["url"] = "data:image/png;base64," + imageBase64;
        imagePart["image_url"] = imageUrl;
        content.append(imagePart);
    }

    message["content"] = content;
    messages.append(message);
    payload["messages"] = messages;
    
    // 添加温度参数
    payload["temperature"] = m_temperature;
    
    // 可选参数：max_tokens
    if (m_config.params.contains("max_tokens"))
    {
        bool ok;
        int maxTokens = m_config.params["max_tokens"].toInt(&ok);
        if (ok && maxTokens > 0)
        {
            payload["max_tokens"] = maxTokens;
        }
    }

    // 转换为 JSON 格式
    QJsonDocument requestDoc(payload);
    QByteArray requestData = requestDoc.toJson(QJsonDocument::Compact);

    qDebug() << "CustomAdapter: 提示词:" << finalPrompt;

    // 打印请求体（调试用，限制长度）
    if (requestData.size() < 1000)
    {
        qDebug() << "CustomAdapter: 完整请求体:" << QString::fromUtf8(requestData);
    }
    else
    {
        QString preview = QString::fromUtf8(requestData.left(300)) +
                          "\n... [省略 " + QString::number(requestData.size() - 600) + " 字节] ...\n" +
                          QString::fromUtf8(requestData.right(300));
        qDebug() << "CustomAdapter: 请求体预览:" << preview;
    }

    qDebug() << "";
    qDebug() << "CustomAdapter: 发送 HTTP POST 请求...";

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
                qDebug() << "CustomAdapter: 收到错误信号:" << error;
            });

    // 设置超时（60秒）
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, [&loop, reply]() {
        qWarning() << "CustomAdapter: 请求超时！";
        loop.quit();
    });
    timer.start(60000);

    qDebug() << "CustomAdapter: 等待响应... (超时: 60秒)";

    QElapsedTimer requestTimer;
    requestTimer.start();

    loop.exec();

    qint64 requestTime = requestTimer.elapsed();
    qDebug() << "CustomAdapter: 请求耗时:" << requestTime << "ms";

    // 检查是否超时
    if (!reply->isFinished())
    {
        reply->abort();
        reply->deleteLater();
        qWarning() << "CustomAdapter: 请求超时！";
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
    
    qDebug() << "CustomAdapter: 收到响应";
    qDebug() << "CustomAdapter: HTTP 状态码:" << (statusCode > 0 ? QString::number(statusCode) : "未获取到");
    
    // 如果有响应数据，打印前500字符用于调试
    if (!responseData.isEmpty()) {
        QString responsePreview = QString::fromUtf8(responseData);
        if (responsePreview.length() > 500) {
            responsePreview = responsePreview.left(500) + "... [省略 " + QString::number(responsePreview.length() - 500) + " 字符]";
        }
        qDebug() << "CustomAdapter: 响应内容预览:" << responsePreview;
    }

    reply->deleteLater();

    // 检查 HTTP 状态码
    if (statusCode > 0 && statusCode != 200)
    {
        errorMsg = QString("HTTP 错误 %1").arg(statusCode);
        qWarning() << "CustomAdapter: HTTP 错误:" << statusCode;
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
            qWarning() << "CustomAdapter: 响应内容:" << QString::fromUtf8(responseData);
        }
        return QString();
    }

    if (hasNetworkError && responseData.isEmpty())
    {
        errorMsg = QString("网络错误: %1").arg(reply->errorString());
        qWarning() << "CustomAdapter: 网络错误:" << errorMsg;
        return QString();
    }

    qDebug() << "";
    qDebug() << "CustomAdapter: 解析 API 响应...";

    // 解析响应
    QString result = parseAPIResponse(responseData, errorMsg);

    if (!result.isEmpty())
    {
        qDebug() << "CustomAdapter: 解析成功，提取到文本:" << result.length() << "字符";
    }
    else if (!errorMsg.isEmpty())
    {
        qWarning() << "CustomAdapter: 解析失败:" << errorMsg;
    }

    return result;
}

QString CustomAdapter::parseAPIResponse(const QByteArray& response, QString& errorMsg)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(response, &parseError);
    
    if (parseError.error != QJsonParseError::NoError)
    {
        errorMsg = QString("JSON 解析错误: %1").arg(parseError.errorString());
        qWarning() << "CustomAdapter: JSON 解析失败:" << errorMsg;
        return QString();
    }
    
    if (!doc.isObject())
    {
        errorMsg = "响应格式错误: 根元素不是对象";
        qWarning() << "CustomAdapter:" << errorMsg;
        return QString();
    }
    
    QJsonObject root = doc.object();
    
    // 检查是否有错误
    if (root.contains("error"))
    {
        QJsonObject errorObj = root["error"].toObject();
        QString errorMessage = errorObj.value("message").toString();
        QString errorType = errorObj.value("type").toString();
        errorMsg = QString("API 错误 (%1): %2").arg(errorType).arg(errorMessage);
        qWarning() << "CustomAdapter:" << errorMsg;
        return QString();
    }
    
    // 解析 OpenAI 兼容格式的响应
    // 格式: { "choices": [ { "message": { "content": "..." } } ] }
    if (!root.contains("choices") || !root["choices"].isArray())
    {
        errorMsg = "响应格式错误: 缺少 choices 数组";
        qWarning() << "CustomAdapter:" << errorMsg;
        return QString();
    }
    
    QJsonArray choices = root["choices"].toArray();
    if (choices.isEmpty())
    {
        errorMsg = "响应格式错误: choices 数组为空";
        qWarning() << "CustomAdapter:" << errorMsg;
        return QString();
    }
    
    QJsonObject firstChoice = choices[0].toObject();
    if (!firstChoice.contains("message") || !firstChoice["message"].isObject())
    {
        errorMsg = "响应格式错误: 缺少 message 对象";
        qWarning() << "CustomAdapter:" << errorMsg;
        return QString();
    }
    
    QJsonObject message = firstChoice["message"].toObject();
    if (!message.contains("content") || !message["content"].isString())
    {
        errorMsg = "响应格式错误: 缺少 content 字段";
        qWarning() << "CustomAdapter:" << errorMsg;
        return QString();
    }
    
    QString content = message["content"].toString();
    qDebug() << "CustomAdapter: 成功提取内容，长度:" << content.length();
    
    return content;
}

OCRResult CustomAdapter::recognize(const QImage &image, const QString &prompt)
{
    QMutexLocker locker(&m_mutex);
    QElapsedTimer timer;
    timer.start();

    qDebug() << "";
    qDebug() << "=== 开始 Custom OCR 识别 ===";

    OCRResult result;
    result.modelName = m_config.displayName;

    if (!m_initialized)
    {
        result.success = false;
        result.errorMessage = "Custom 模型未初始化";
        qWarning() << "CustomAdapter: 错误: Custom 模型未初始化";
        return result;
    }

    // 判断是否有有效图片（用于纯文本询问AI）
    bool hasValidImage = !image.isNull() && !(image.width() <= 1 && image.height() <= 1);
    
    if (!hasValidImage)
    {
        qDebug() << "CustomAdapter: 无有效图片，将进行纯文本AI询问";
    }

    qDebug() << "CustomAdapter: 图片信息:";
    if (hasValidImage) {
        qDebug() << "CustomAdapter: 尺寸:" << image.width() << "x" << image.height();
        qDebug() << "CustomAdapter: 格式:" << image.format();
    } else {
        qDebug() << "CustomAdapter: 无图片（纯文本模式）";
    }
    qDebug() << "CustomAdapter: 提示词:" << (prompt.isEmpty() ? "默认" : prompt);

    try
    {
        QString imageBase64;
        if (hasValidImage) {
            // 编码图片
            qDebug() << "";
            qDebug() << "CustomAdapter: 步骤 1/2: 编码图片为 base64...";
            imageBase64 = encodeImageToBase64(image);
            qDebug() << "CustomAdapter: 图片编码完成";
            qDebug() << "CustomAdapter: Base64 长度:" << imageBase64.length() << "字符";
        }

        // 调用 API（不传图片参数，如果图片为空）
        qDebug() << "";
        qDebug() << "CustomAdapter: 步骤" << (hasValidImage ? "2/2" : "1/1") << ": 调用 API...";
        QString errorMsg;
        QString text = callAPI(imageBase64, prompt, hasValidImage, errorMsg);

        if (text.isEmpty() && !errorMsg.isEmpty())
        {
            result.success = false;
            result.errorMessage = errorMsg;
            qWarning() << "CustomAdapter: API 调用失败:" << errorMsg;
            qDebug() << "====================================";
            return result;
        }

        // 构建结果
        qDebug() << "";
        qDebug() << "CustomAdapter: 处理结果...";
        result.success = true;
        result.fullText = text.trimmed();

        // 创建文本块
        if (!result.fullText.isEmpty())
        {
            TextBlock block;
            block.text = result.fullText;
            block.boundingBox = QRectF(0, 0, 1, 1); // 整个图像
            block.confidence = 0.95f;               // 假设较高准确度
            result.textBlocks.append(block);

            qDebug() << "CustomAdapter: 识别成功!";
            qDebug() << "CustomAdapter: 文本长度:" << result.fullText.length() << "字符";
            qDebug() << "CustomAdapter: 文本块数:" << result.textBlocks.size();
        }
        else
        {
            qDebug() << "CustomAdapter: 警告: 识别结果为空（图片可能没有文字）";
        }

        result.processingTimeMs = timer.elapsed();

        qDebug() << "";
        qDebug() << "CustomAdapter: OCR 完成! 总耗时:" << result.processingTimeMs << "ms";
        qDebug() << "====================================";
    }
    catch (const std::exception &e)
    {
        result.success = false;
        result.errorMessage = QString("异常: ") + e.what();
        qWarning() << "CustomAdapter: 异常:" << e.what();
        qDebug() << "====================================";
    }
    catch (...)
    {
        result.success = false;
        result.errorMessage = "未知异常";
        qWarning() << "CustomAdapter: 未知异常";
        qDebug() << "====================================";
    }

    return result;
}