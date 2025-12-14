#pragma once
#include "../core/ModelAdapter.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMutex>
#include <QEventLoop>
#include <QThread>

// Qwen 模型适配器
// 支持通过 online/local API 调用 Qwen 视觉语言模型进行 OCR
class QwenAdapter : public ModelAdapter {
    Q_OBJECT
    
public:
    explicit QwenAdapter(const ModelConfig& config, QObject* parent = nullptr);
    ~QwenAdapter() override;
    
    bool initialize() override;
    OCRResult recognize(const QImage& image, const QString& prompt = QString()) override;
    bool isInitialized() const override { return m_initialized; }
    
private:
    // 将 QImage 编码为 base64 格式
    QString encodeImageToBase64(const QImage& image);
    
    // 调用 Qwen VL API
    // hasImage: 是否有有效图片（如果有图片才添加到请求中）
    QString callVLAPI(const QString& imageBase64, const QString& prompt, bool hasImage, QString& errorMsg);
    
    // 解析 API 响应
    QString parseAPIResponse(const QByteArray& response, QString& errorMsg);
    
    bool m_initialized;
    QString m_modelName;        // 模型名称 (如 qwen-vl-plus)
    QString m_apiKey;           // API 密钥
    QString m_apiUrl;           // API 地址
    QString m_deployType;       // 部署类型 (online/local)
    float m_temperature;        // 温度参数
    bool m_enableThinking;      // 思考模式开关
    
    QNetworkAccessManager* m_networkManager;  // 网络管理器
    QMutex m_mutex;             // 线程安全
};