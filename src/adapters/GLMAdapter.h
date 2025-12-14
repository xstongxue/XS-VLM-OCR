#pragma once
#include "../core/ModelAdapter.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMutex>
#include <QEventLoop>
#include <QThread>

// GLM 模型适配器（智谱清言）
// 支持调用智谱清言的 GLM 模型进行 OCR
class GLMAdapter : public ModelAdapter {
    Q_OBJECT
    
public:
    explicit GLMAdapter(const ModelConfig& config, QObject* parent = nullptr);
    ~GLMAdapter() override;
    
    bool initialize() override;
    OCRResult recognize(const QImage& image, const QString& prompt = QString()) override;
    bool isInitialized() const override { return m_initialized; }
    
private:
    // 将 QImage 编码为 base64 格式
    QString encodeImageToBase64(const QImage& image);
    
    // 调用 GLM API
    // hasImage: 是否有有效图片（如果有图片才添加到请求中）
    QString callAPI(const QString& imageBase64, const QString& prompt, bool hasImage, QString& errorMsg);
    
    // 解析 API 响应
    QString parseAPIResponse(const QByteArray& response, QString& errorMsg);
    
    bool m_initialized;
    QString m_modelName;        // 模型名称 (glm-4.6v 或 glm-4.5v)
    QString m_apiKey;           // API 密钥
    QString m_apiUrl;           // API 地址
    float m_temperature;        // 温度参数
    bool m_enableThinking;      // 是否启用思考过程
    
    QNetworkAccessManager* m_networkManager;  // 网络管理器
    QMutex m_mutex;             // 线程安全
};

