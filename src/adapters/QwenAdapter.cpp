#include "QwenAdapter.h"
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

QwenAdapter::QwenAdapter(const ModelConfig &config, QObject *parent)
    : ModelAdapter(config, parent), m_initialized(false), m_networkManager(nullptr), m_temperature(0.1f), m_enableThinking(false)
{
    // 从配置中读取参数
    m_modelName = m_config.params.value("model_name", "qwen-vl-plus");
    m_apiKey = m_config.params.value("api_key", "");
    m_deployType = m_config.params.value("deploy_type", "online");

    // 默认 API 地址
    QString defaultHost = "https://dashscope.aliyuncs.com/compatible-mode";
    QString host = m_config.params.value("api_host", defaultHost);
    
    // 检查 host 是否已经包含 /v1/chat/completions 路径
    if (host.endsWith("/v1/chat/completions"))
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
    
    // 思考模式参数（默认 false）
    QString thinkingStr = m_config.params.value("enable_thinking", "false").toLower();
    m_enableThinking = (thinkingStr == "true" || thinkingStr == "1");
}

QwenAdapter::~QwenAdapter()
{
    if (m_networkManager)
    {
        delete m_networkManager;
    }
}

bool QwenAdapter::initialize()
{
    QMutexLocker locker(&m_mutex);

    qDebug() << "=== Initializing QwenAdapter ===";
    qDebug() << "Model:" << m_modelName;
    qDebug() << "Deploy Type:" << m_deployType;
    qDebug() << "API URL:" << m_apiUrl;
    qDebug() << "API Key:" << (m_apiKey.isEmpty() ? "NOT SET" : QString("sk-***") + m_apiKey.right(4));

    // 检查必要的配置
    if (m_deployType == "online" && m_apiKey.isEmpty())
    {
        qWarning() << "QwenAdapter: API key 需要在线部署";
        qWarning() << "请在配置中设置 api_key 参数";
        return false;
    }

    if (m_apiUrl.isEmpty())
    {
        qWarning() << "QwenAdapter: API URL 为空";
        return false;
    }

    // 注意：不在这里创建 QNetworkAccessManager
    // 它会在 recognize() 方法中的工作线程里创建
    m_initialized = true;
    qDebug() << "QwenAdapter: 初始化成功！";
    return true;
}

QString QwenAdapter::encodeImageToBase64(const QImage &image)
{
    // 将图片转换为 PNG 格式的 base64（兼容性更好）
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);

    // 优先使用 PNG 格式（无损，兼容性好）
    // 如果图片太大，使用 JPEG 压缩
    if (image.width() * image.height() > 1920 * 1080)
    {
        image.save(&buffer, "JPEG", 85);
        qDebug() << "QwenAdapter: 使用 JPEG 压缩（图片较大）";
    }
    else
    {
        image.save(&buffer, "PNG");
        qDebug() << "QwenAdapter: 使用 PNG 格式";
    }

    // 转换为 base64（不添加前缀，直接返回纯 base64）
    QString base64 = byteArray.toBase64();

    // qDebug() << "  - 原始图片大小:" << (byteArray.size() / 1024.0) << "KB";

    return base64;
}

QString QwenAdapter::parseAPIResponse(const QByteArray &response, QString &errorMsg)
{
    // 解析 JSON 响应
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(response, &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        errorMsg = "JSON 解析错误: " + parseError.errorString();
        qWarning() << "QwenAdapter: 解析 JSON 失败:" << parseError.errorString();
        qWarning() << "响应:" << response;
        return QString();
    }

    if (!doc.isObject())
    {
        errorMsg = "无效的响应格式: 不是 JSON 对象";
        return QString();
    }

    QJsonObject obj = doc.object();

    // 检查是否有错误
    if (obj.contains("error"))
    {
        QJsonObject error = obj["error"].toObject();
        errorMsg = "API 错误: " + error["message"].toString();
        qWarning() << "QwenAdapter: API 错误:" << errorMsg;
        return QString();
    }

    // 解析 choices
    if (!obj.contains("choices") || !obj["choices"].isArray())
    {
        errorMsg = "响应中缺少 choices 字段";
        qWarning() << "QwenAdapter: 响应中缺少 choices 字段";
        return QString();
    }

    QJsonArray choices = obj["choices"].toArray();
    if (choices.isEmpty())
    {
        errorMsg = "choices 数组为空";
        return QString();
    }

    // 获取第一个 choice
    QJsonObject choice = choices[0].toObject();
    if (!choice.contains("message"))
    {
        errorMsg = "choice 中缺少 message 字段";
        return QString();
    }

    QJsonObject message = choice["message"].toObject();
    if (!message.contains("content"))
    {
        errorMsg = "message 中缺少 content 字段";
        return QString();
    }

    QString content = message["content"].toString();
    return content;
}

QString QwenAdapter::callVLAPI(const QString &imageBase64, const QString &prompt, bool hasImage, QString &errorMsg)
{
    // 在当前线程创建 QNetworkAccessManager（重要！）
    QNetworkAccessManager networkManager;

    qDebug() << "QwenAdapter: 准备 API 请求...";
    qDebug() << "QwenAdapter: 当前线程ID:" << QThread::currentThreadId();
    qDebug() << "QwenAdapter: API URL:" << m_apiUrl;
    qDebug() << "QwenAdapter: 模型:" << m_modelName;
    qDebug() << "QwenAdapter: 温度:" << m_temperature;
    qDebug() << "QwenAdapter: 思考模式:" << (m_enableThinking ? "启用" : "关闭");

    // 构建请求
    QNetworkRequest request;
    QUrl url(m_apiUrl);
    request.setUrl(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    // 设置连接属性
    request.setRawHeader("Connection", "keep-alive");
    request.setRawHeader("Accept", "application/json");

    // 设置认证头（如果是在线部署）
    if (m_deployType == "online")
    {
        QString authHeader = QString("Bearer %1").arg(m_apiKey);
        request.setRawHeader("Authorization", authHeader.toUtf8());
        qDebug() << "QwenAdapter: 认证: Bearer sk-***" << m_apiKey.right(4);
    }

    // 构建请求体
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
    
    // 添加思考模式参数（如果启用）
    if (m_enableThinking) {
        payload["enable_thinking"] = true;
    }

    // 转换为 JSON 格式
    QJsonDocument requestDoc(payload);
    QByteArray requestData = requestDoc.toJson(QJsonDocument::Compact);

    // qDebug() << "  - 请求体大小:" << (requestData.size() / 1024.0) << "KB";
    qDebug() << "QwenAdapter: 提示词:" << finalPrompt;

    // 打印请求体（调试用，限制长度）
    if (requestData.size() < 1000)
    {
        qDebug() << "QwenAdapter: 完整请求体:" << QString::fromUtf8(requestData);
    }
    else
    {
        QString preview = QString::fromUtf8(requestData.left(300)) +
                          "\n... [省略 " + QString::number(requestData.size() - 600) + " 字节] ...\n" +
                          QString::fromUtf8(requestData.right(300));
        qDebug() << "QwenAdapter: 请求体预览:" << preview;
    }

    qDebug() << "";
    qDebug() << "QwenAdapter: 发送 HTTP POST 请求...";

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
        // qDebug() << "  - 收到数据块:" << newData.size() << "字节，累计:" << responseData.size() << "字节";
    });
    
    // 连接多个信号，确保能捕获所有情况
    connect(reply, &QNetworkReply::finished, [&loop, &responseData, reply, &hasReceivedData]() {
        // 确保读取所有剩余数据
        if (reply->bytesAvailable() > 0) {
            QByteArray remaining = reply->readAll();
            responseData.append(remaining);
            hasReceivedData = true;
            // qDebug() << "  - finished时读取剩余数据:" << remaining.size() << "字节";
        }
        // qDebug() << "  - 收到finished信号，总数据大小:" << responseData.size() << "字节，是否收到过数据:" << hasReceivedData;
        loop.quit();
    });
    
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::error),
            [](QNetworkReply::NetworkError error) {
                qDebug() << "QwenAdapter: 收到错误信号:" << error;
            });

    // 设置超时（60秒）
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, [&loop, reply]() {
        qWarning() << "QwenAdapter: 请求超时！";
        loop.quit();
    });
    timer.start(60000);

    qDebug() << "QwenAdapter: 等待响应... (超时: 60秒)";

    QElapsedTimer requestTimer;
    requestTimer.start();

    loop.exec();

    qint64 requestTime = requestTimer.elapsed();
    qDebug() << "QwenAdapter: 请求耗时:" << requestTime << "ms";

    // 检查是否超时
    if (!reply->isFinished())
    {
        reply->abort();
        reply->deleteLater();
        qWarning() << "QwenAdapter: 请求超时！";
        return QString();
    }
    
    // 再次确保读取所有数据（双重保险）
    if (reply->bytesAvailable() > 0) {
        QByteArray remaining = reply->readAll();
        responseData.append(remaining);
        hasReceivedData = true;
        // qDebug() << "  - 最终读取剩余数据:" << remaining.size() << "字节";
    }
    
    // 检查 HTTP 状态码（即使有网络错误也可能有状态码）
    QVariant statusCodeVariant = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    int statusCode = statusCodeVariant.isValid() ? statusCodeVariant.toInt() : 0;
    
    // 检查网络错误
    QNetworkReply::NetworkError networkError = reply->error();
    bool hasNetworkError = (networkError != QNetworkReply::NoError);
    
    qDebug() << "QwenAdapter: 收到响应";
    // qDebug() << "QwenAdapter: 请求完成状态:" << (reply->isFinished() ? "已完成" : "未完成");
    // qDebug() << "QwenAdapter: 网络错误代码:" << networkError << (hasNetworkError ? "（有错误）" : "（无错误）");
    // qDebug() << "QwenAdapter: HTTP 状态码:" << (statusCode > 0 ? QString::number(statusCode) : "未获取到");
    // qDebug() << "QwenAdapter: 响应大小:" << (responseData.size() / 1024.0) << "KB";
    qDebug() << "QwenAdapter: 是否收到过数据:" << hasReceivedData;
    // qDebug() << "QwenAdapter: 剩余可读字节:" << reply->bytesAvailable();
    
    // 打印响应头信息
    // QList<QByteArray> headers = reply->rawHeaderList();
    // if (!headers.isEmpty()) {
    //     qDebug() << "QwenAdapter: 响应头:";
    //     for (const QByteArray& header : headers) {
    //         qDebug() << "    " << header << ":" << reply->rawHeader(header);
    //     }
    // }
    
    // 如果有响应数据，打印前500字符用于调试
    if (!responseData.isEmpty()) {
        QString responsePreview = QString::fromUtf8(responseData);
        if (responsePreview.length() > 500) {
            responsePreview = responsePreview.left(500) + "... [省略 " + QString::number(responsePreview.length() - 500) + " 字符]";
        }
        qDebug() << "QwenAdapter: 响应内容预览:" << responsePreview;
    }

    reply->deleteLater();

    // 如果有网络错误，但收到了响应数据，尝试解析响应
    if (hasNetworkError)
    {
        // 如果收到了响应数据，尝试解析（可能是有效的响应）
        if (!responseData.isEmpty()) {
            qWarning() << "QwenAdapter: 网络层报告错误，但收到了服务器响应数据，尝试解析...";
            // qWarning() << "响应数据大小：" << responseData.size() << "字节";
            
            // 尝试解析响应，看是否是有效的JSON响应
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(responseData, &parseError);
            if (!parseError.error && doc.isObject()) {
                QJsonObject obj = doc.object();
                
                // 检查是否是错误响应
                if (obj.contains("error")) {
                    QJsonObject errorObj = obj["error"].toObject();
                    QString errorMsgFromServer = errorObj.value("message").toString();
                    if (!errorMsgFromServer.isEmpty()) {
                        errorMsg = QString("服务器错误: %1").arg(errorMsgFromServer);
                        qWarning() << "QwenAdapter: API 错误:" << errorMsg;
                        return QString();
                    }
                }
                
                // 如果解析成功且不是错误响应，尝试正常解析（可能是有效的响应）
                qWarning() << "QwenAdapter: 响应数据解析成功，尝试提取内容...";
                QString result = parseAPIResponse(responseData, errorMsg);
                if (!result.isEmpty()) {
                    qWarning() << "QwenAdapter: 成功从错误连接中提取到响应内容！";
                    return result;
                }
            } else {
                qWarning() << "QwenAdapter: 响应不是有效的JSON，原始内容:" << QString::fromUtf8(responseData.left(200));
            }
        }
        
        // 如果没有有效的响应数据，返回网络错误
        errorMsg = QString("QwenAdapter: 网络错误: %1").arg(reply->errorString());
        qWarning() << "❌" << errorMsg;
        qWarning() << "QwenAdapter: 错误代码:" << networkError;
        qWarning() << "QwenAdapter: 提示: 服务器可能收到了请求，但连接在响应过程中被关闭";
        qWarning() << "QwenAdapter: 建议: 检查服务器日志，确认服务器是否正常发送了响应";
        return QString();
    }

    // 检查 HTTP 状态码
    if (statusCode > 0 && statusCode != 200)
    {
        errorMsg = QString("HTTP 错误 %1").arg(statusCode);
        qWarning() << "QwenAdapter: HTTP 错误:" << statusCode;
        if (!responseData.isEmpty()) {
            qWarning() << "QwenAdapter: 响应内容:" << QString::fromUtf8(responseData);
        }
        return QString();
    }

    qDebug() << "QwenAdapter: 响应大小:" << (responseData.size() / 1024.0) << "KB";

    // 打印完整响应用于调试
    qDebug() << "QwenAdapter: 完整响应:" << QString::fromUtf8(responseData);

    if (statusCode != 200)
    {
        errorMsg = QString("HTTP 错误 %1").arg(statusCode);
        qWarning() << "QwenAdapter: HTTP 错误:" << statusCode;
        qWarning() << "QwenAdapter: 响应内容:" << QString::fromUtf8(responseData);
        return QString();
    }

    qDebug() << "";
    qDebug() << "QwenAdapter: 解析 API 响应...";

    // 解析响应
    QString result = parseAPIResponse(responseData, errorMsg);

    if (!result.isEmpty())
    {
        qDebug() << "QwenAdapter: 解析成功，提取到文本:" << result.length() << "字符";
    }
    else if (!errorMsg.isEmpty())
    {
        qWarning() << "QwenAdapter: 解析失败:" << errorMsg;
    }

    return result;
}

OCRResult QwenAdapter::recognize(const QImage &image, const QString &prompt)
{
    QMutexLocker locker(&m_mutex);
    QElapsedTimer timer;
    timer.start();

    qDebug() << "";
    qDebug() << "=== 开始 Qwen OCR 识别 ===";

    OCRResult result;
    result.modelName = m_config.displayName;

    if (!m_initialized)
    {
        result.success = false;
        result.errorMessage = "Qwen 模型未初始化";
        qWarning() << "QwenAdapter: 错误: Qwen 模型未初始化";
        return result;
    }

    // 判断是否有有效图片（用于纯文本询问AI）
    bool hasValidImage = !image.isNull() && !(image.width() <= 1 && image.height() <= 1);
    
    if (!hasValidImage)
    {
        qDebug() << "QwenAdapter: 无有效图片，将进行纯文本AI询问";
    }

    qDebug() << "QwenAdapter: 图片信息:";
    if (hasValidImage) {
        qDebug() << "QwenAdapter: 尺寸:" << image.width() << "x" << image.height();
        qDebug() << "QwenAdapter: 格式:" << image.format();
    } else {
        qDebug() << "QwenAdapter: 无图片（纯文本模式）";
    }
    qDebug() << "QwenAdapter: 提示词:" << (prompt.isEmpty() ? "默认" : prompt);

    try
    {
        QString imageBase64;
        if (hasValidImage) {
            // 编码图片
            qDebug() << "";
            qDebug() << "QwenAdapter: 步骤 1/2: 编码图片为 base64...";
            imageBase64 = encodeImageToBase64(image);
            qDebug() << "QwenAdapter: 图片编码完成";
            qDebug() << "QwenAdapter: Base64 长度:" << imageBase64.length() << "字符";
            qDebug() << "QwenAdapter: 预计大小:" << (imageBase64.length() / 1024.0) << "KB";
        }

        // 调用 API（不传图片参数，如果图片为空）
        qDebug() << "";
        qDebug() << "QwenAdapter: 步骤" << (hasValidImage ? "2/2" : "1/1") << ": 调用 Qwen API...";
        QString errorMsg;
        QString text = callVLAPI(imageBase64, prompt, hasValidImage, errorMsg);

        if (text.isEmpty() && !errorMsg.isEmpty())
        {
            result.success = false;
            result.errorMessage = errorMsg;
            qWarning() << "QwenAdapter: Qwen API 调用失败:" << errorMsg;
            qDebug() << "====================================";
            return result;
        }

        // 构建结果
        qDebug() << "";
        qDebug() << "QwenAdapter: 步骤 3/3: 处理识别结果...";
        result.success = true;
        result.fullText = text.trimmed();

        // 创建文本块
        if (!result.fullText.isEmpty())
        {
            TextBlock block;
            block.text = result.fullText;
            block.boundingBox = QRectF(0, 0, 1, 1); // 整个图像
            block.confidence = 0.95f;               // Qwen 通常有较高的准确度
            result.textBlocks.append(block);

            qDebug() << "QwenAdapter: 识别成功!";
            qDebug() << "QwenAdapter: 文本长度:" << result.fullText.length() << "字符";
            qDebug() << "QwenAdapter: 文本块数:" << result.textBlocks.size();
        }
        else
        {
            qDebug() << "QwenAdapter: 警告: 识别结果为空（图片可能没有文字）";
        }

        result.processingTimeMs = timer.elapsed();

        qDebug() << "";
        qDebug() << "QwenAdapter: OCR 完成! 总耗时:" << result.processingTimeMs << "ms";
        qDebug() << "====================================";
    }
    catch (const std::exception &e)
    {
        result.success = false;
        result.errorMessage = QString("异常: ") + e.what();
        qWarning() << "QwenAdapter: 异常:" << e.what();
        qDebug() << "====================================";
    }
    catch (...)
    {
        result.success = false;
        result.errorMessage = "未知异常";
        qWarning() << "QwenAdapter: 未知异常";
        qDebug() << "====================================";
    }

    return result;
}