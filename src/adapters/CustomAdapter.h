#pragma once
#include "../core/ModelAdapter.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMutex>
#include <QEventLoop>
#include <QThread>

// Custom 模型适配器（支持 OpenAI 兼容格式）
// 支持通过兼容 OpenAI API 格式的接口调用任意模型进行 OCR
class CustomAdapter : public ModelAdapter {
    Q_OBJECT
    
public:
    explicit CustomAdapter(const ModelConfig& config, QObject* parent = nullptr);
    ~CustomAdapter() override;
    
    bool initialize() override;
    OCRResult recognize(const QImage& image, const QString& prompt = QString()) override;
    bool isInitialized() const override { return m_initialized; }
    
private:
    // 将 QImage 编码为 base64 格式
    QString encodeImageToBase64(const QImage& image);
    
    // 调用 OpenAI 兼容 API
    // hasImage: 是否有有效图片（如果有图片才添加到请求中）
    QString callAPI(const QString& imageBase64, const QString& prompt, bool hasImage, QString& errorMsg);
    
    // 解析 API 响应
    QString parseAPIResponse(const QByteArray& response, QString& errorMsg);
    
    bool m_initialized;
    QString m_modelName;        // 模型名称
    QString m_apiKey;           // API 密钥
    QString m_apiUrl;           // API 地址
    float m_temperature;        // 温度参数
    QString m_contentType;      // Content-Type 设置（用于支持不同的图片格式）
    
    QNetworkAccessManager* m_networkManager;  // 网络管理器
    QMutex m_mutex;             // 线程安全
};

