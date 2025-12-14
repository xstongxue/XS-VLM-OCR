#include "PaddleAdapter.h"
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

PaddleAdapter::PaddleAdapter(const ModelConfig &config, QObject *parent)
    : ModelAdapter(config, parent), m_initialized(false), m_networkManager(nullptr)
{
    // 从配置中读取参数
    // api_key 可能已经从 provider 继承（通过 ConfigManager）
    m_apiKey = m_config.params.value("api_key");
    
    // api_url 优先，如果没有则尝试 api_host（从 provider 继承的）
    m_apiUrl = m_config.params.value("api_url");
    if (m_apiUrl.isEmpty()) {
        m_apiUrl = m_config.params.value("api_host");
    }
}

PaddleAdapter::~PaddleAdapter()
{
    if (m_networkManager) {
        delete m_networkManager;
        m_networkManager = nullptr;
    }
}

bool PaddleAdapter::initialize()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_initialized) {
        return true;
    }
    
    qDebug() << "=== Initializing PaddleAdapter ===";
    qDebug() << "Type:" << m_config.type;
    qDebug() << "API URL:" << m_apiUrl;
    qDebug() << "API Key:" << (m_apiKey.isEmpty() ? "NOT SET" : QString("***") + m_apiKey.right(4));
    
    // 检查必需参数
    if (m_apiKey.isEmpty()) {
        qWarning() << "PaddleAdapter: API Token 未设置";
        qWarning() << "请在配置中设置 api_key 参数";
        return false;
    }
    
    if (m_apiUrl.isEmpty()) {
        qWarning() << "PaddleAdapter: API URL 未设置";
        qWarning() << "请在配置中设置 api_url 或 api_host 参数";
        return false;
    }
    
    qDebug() << "PaddleAdapter: 初始化成功！";
    
    m_initialized = true;
    return true;
}

OCRResult PaddleAdapter::recognize(const QImage &image, const QString &prompt)
{
    QMutexLocker locker(&m_mutex);
    QElapsedTimer timer;
    timer.start();

    qDebug() << "";
    qDebug() << "=== 开始 PaddleOCR 识别 ===";

    OCRResult result;
    result.modelName = m_config.displayName;
    
    if (!m_initialized) {
        result.success = false;
        result.errorMessage = "PaddleOCR 模型未初始化";
        qWarning() << "PaddleAdapter: 错误: PaddleOCR 模型未初始化";
        return result;
    }

    // PaddleOCR 需要图片
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        result.success = false;
        result.errorMessage = "图片无效";
        qWarning() << "PaddleAdapter: 错误: 图片无效";
        return result;
    }
    
    // 编码图片
    QString imageBase64 = encodeImageToBase64(image);
    if (imageBase64.isEmpty()) {
        result.success = false;
        result.errorMessage = "图片编码失败";
        qWarning() << "PaddleAdapter: 错误: 图片编码失败";
        return result;
    }

    QString errorMsg;
    QString text = callAPI(imageBase64, errorMsg);

    if (text.isEmpty() && !errorMsg.isEmpty()) {
        result.success = false;
        result.errorMessage = errorMsg;
        qWarning() << "PaddleAdapter: 错误:" << errorMsg;
        return result;
    }

    result.fullText = text;
    result.success = true;
    result.processingTimeMs = timer.elapsed();
    
    qDebug() << "PaddleAdapter: 识别成功，文本长度:" << text.length() << "字符，耗时:" << result.processingTimeMs << "ms";
    
    return result;
}

QString PaddleAdapter::encodeImageToBase64(const QImage &image)
{
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    
    // 转换为 PNG 格式
    if (!image.save(&buffer, "PNG")) {
        qWarning() << "PaddleAdapter: 图片保存失败";
        return QString();
    }
    
    QByteArray imageData = buffer.data();
    QByteArray base64Data = imageData.toBase64();
    
    return QString::fromLatin1(base64Data);
}

QString PaddleAdapter::callAPI(const QString& imageBase64, QString& errorMsg)
{
    // 在工作线程中创建网络管理器
    QNetworkAccessManager networkManager;
    
    qDebug() << "";
    qDebug() << "=== PaddleAdapter: 调用 PaddleOCR API ===";

    // 构建请求
    QNetworkRequest request;
    QUrl url(m_apiUrl);
    request.setUrl(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    // 设置连接属性
    request.setRawHeader("Connection", "keep-alive");
    request.setRawHeader("Accept", "application/json");

    // 设置认证头（token）
    if (!m_apiKey.isEmpty()) {
        QString authHeader = QString("token %1").arg(m_apiKey);
        request.setRawHeader("Authorization", authHeader.toUtf8());
        qDebug() << "PaddleAdapter: 认证: token ***" << m_apiKey.right(4);
    } else {
        qDebug() << "PaddleAdapter: 未设置 API Key";
        errorMsg = "API Key 未设置";
        return QString();
    }

    // 构建请求体（PaddleOCR API 格式）
    QJsonObject payload;
    payload["file"] = imageBase64;        // Base64 编码的图片
    payload["fileType"] = 1;              // 1 表示图像文件

    // 可选参数, 可以根据需要添加这些参数：
    // payload["useDocOrientationClassify"] = false;  // 文档方向分类
    // payload["useDocUnwarping"] = false;  // 文档纠偏
    // payload["useLayoutDetection"] = true;  // 布局检测
    // payload["useChartRecognition"] = false;  // 图表识别

    // 转换为 JSON 格式
    QJsonDocument requestDoc(payload);
    QByteArray requestData = requestDoc.toJson(QJsonDocument::Compact);

    qDebug() << "PaddleAdapter: 请求体大小:" << requestData.size() << "字节";
    qDebug() << "PaddleAdapter: 图片 Base64 长度:" << imageBase64.length();

    qDebug() << "";
    qDebug() << "PaddleAdapter: 发送 HTTP POST 请求...";

    // 发送请求
    QNetworkReply *reply = networkManager.post(request, requestData);

    // 等待响应（同步方式）
    QEventLoop loop;
    QByteArray responseData;
    
    // 在readyRead时累积读取数据
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
                qDebug() << "PaddleAdapter: 收到错误信号:" << error;
            });

    // 设置超时（60秒）
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, [&loop, reply]() {
        qWarning() << "PaddleAdapter: 请求超时！";
        loop.quit();
    });
    timer.start(60000);

    qDebug() << "PaddleAdapter: 等待响应... (超时: 60秒)";

    QElapsedTimer requestTimer;
    requestTimer.start();

    loop.exec();

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    bool hasNetworkError = (reply->error() != QNetworkReply::NoError);

    qDebug() << "PaddleAdapter: 请求耗时:" << requestTimer.elapsed() << "ms";
    
    if (hasReceivedData || !responseData.isEmpty()) {
        qDebug() << "PaddleAdapter: 收到响应";
        QString responsePreview;
        if (responseData.size() < 1000) {
            responsePreview = QString::fromUtf8(responseData);
        } else {
            responsePreview = QString::fromUtf8(responseData.left(500)) + 
                              "\n... [省略 " + QString::number(responseData.size() - 1000) + " 字节] ...\n" +
                              QString::fromUtf8(responseData.right(500));
        }
        qDebug() << "PaddleAdapter: 响应内容预览:" << responsePreview;
    }

    reply->deleteLater();

    // 检查 HTTP 状态码
    if (statusCode > 0 && statusCode != 200) {
        errorMsg = QString("HTTP 错误 %1").arg(statusCode);
        qWarning() << "PaddleAdapter: HTTP 错误:" << statusCode;
        if (!responseData.isEmpty()) {
            // 尝试解析错误响应
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(responseData, &parseError);
            if (!parseError.error && doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.contains("errorMsg")) {
                    QString errorMsgFromServer = obj["errorMsg"].toString();
                    int errorCode = obj["errorCode"].toInt();
                    if (!errorMsgFromServer.isEmpty()) {
                        errorMsg = QString("HTTP %1 (错误码 %2): %3").arg(statusCode).arg(errorCode).arg(errorMsgFromServer);
                    }
                }
            }
            qWarning() << "PaddleAdapter: 响应内容:" << QString::fromUtf8(responseData);
        }
        return QString();
    }

    if (hasNetworkError && responseData.isEmpty()) {
        errorMsg = QString("网络错误: %1").arg(reply->errorString());
        qWarning() << "PaddleAdapter: 网络错误:" << errorMsg;
        return QString();
    }

    qDebug() << "";
    qDebug() << "PaddleAdapter: 解析 API 响应...";

    // 解析响应
    QString result = parseAPIResponse(responseData, errorMsg);

    if (!result.isEmpty()) {
        qDebug() << "PaddleAdapter: 解析成功，提取到文本:" << result.length() << "字符";
    } else if (!errorMsg.isEmpty()) {
        qWarning() << "PaddleAdapter: 解析失败:" << errorMsg;
    }

    return result;
}

QString PaddleAdapter::parseAPIResponse(const QByteArray& response, QString& errorMsg)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(response, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        errorMsg = QString("JSON 解析错误: %1").arg(parseError.errorString());
        qWarning() << "PaddleAdapter: JSON 解析失败:" << errorMsg;
        return QString();
    }
    
    if (!doc.isObject()) {
        errorMsg = "响应不是有效的 JSON 对象";
        qWarning() << "PaddleAdapter: 响应不是 JSON 对象";
        return QString();
    }

    QJsonObject obj = doc.object();
    
    // PaddleOCR API 响应格式
    // {
    //   "logId": "...",
    //   "errorCode": 0,
    //   "errorMsg": "Success",
    //   "result": {
    //     "layoutParsingResults": [
    //       {
    //         "markdown": {
    //           "text": "..."
    //         }
    //       }
    //     ]
    //   }
    // }
    
    // 检查错误码
    int errorCode = obj["errorCode"].toInt();
    if (errorCode != 0) {
        QString errorMsgFromServer = obj["errorMsg"].toString();
        errorMsg = QString("API 错误 (错误码 %1): %2").arg(errorCode).arg(errorMsgFromServer.isEmpty() ? "未知错误" : errorMsgFromServer);
        qWarning() << "PaddleAdapter:" << errorMsg;
        return QString();
    }
    
    if (!obj.contains("result")) {
        errorMsg = "响应中缺少 result 字段";
        qWarning() << "PaddleAdapter: 响应中缺少 result 字段";
        return QString();
    }

    QJsonObject resultObj = obj["result"].toObject();
    
    if (!resultObj.contains("layoutParsingResults")) {
        errorMsg = "响应中缺少 layoutParsingResults 字段";
        qWarning() << "PaddleAdapter: 响应中缺少 layoutParsingResults 字段";
        return QString();
    }

    QJsonArray layoutResults = resultObj["layoutParsingResults"].toArray();
    if (layoutResults.isEmpty()) {
        errorMsg = "layoutParsingResults 数组为空";
        qWarning() << "PaddleAdapter: layoutParsingResults 数组为空";
        return QString();
    }

    // 获取第一个结果（对于图片输入，数组长度为1）
    QJsonObject firstResult = layoutResults[0].toObject();
    if (!firstResult.contains("markdown")) {
        errorMsg = "结果中缺少 markdown 字段";
        qWarning() << "PaddleAdapter: 结果中缺少 markdown 字段";
        return QString();
    }

    QJsonObject markdown = firstResult["markdown"].toObject();
    QString text = markdown["text"].toString();

    if (text.isEmpty()) {
        errorMsg = "markdown.text 为空";
        qWarning() << "PaddleAdapter: markdown.text 为空";
        return QString();
    }

    // 返回 Markdown 文本（去除首尾空白）
    return text.trimmed();
}