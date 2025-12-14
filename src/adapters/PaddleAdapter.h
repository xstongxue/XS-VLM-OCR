#pragma once
#include "../core/ModelAdapter.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMutex>
#include <QEventLoop>
#include <QThread>

// PaddleOCR 模型适配器
// 支持调用 PaddleOCR API 进行 OCR
class PaddleAdapter : public ModelAdapter {
    Q_OBJECT
    
public:
    explicit PaddleAdapter(const ModelConfig& config, QObject* parent = nullptr);
    ~PaddleAdapter() override;
    
    bool initialize() override;
    OCRResult recognize(const QImage& image, const QString& prompt = QString()) override;
    bool isInitialized() const override { return m_initialized; }
    
private:
    // 将 QImage 编码为 base64 格式
    QString encodeImageToBase64(const QImage& image);
    
    // 调用 PaddleOCR API
    QString callAPI(const QString& imageBase64, QString& errorMsg);
    
    // 解析 API 响应
    QString parseAPIResponse(const QByteArray& response, QString& errorMsg);

    bool m_initialized;
    QString m_apiKey;           // API Token
    QString m_apiUrl;           // API 地址
    
    QNetworkAccessManager* m_networkManager;  // 网络管理器
    QMutex m_mutex;             // 线程安全
};

