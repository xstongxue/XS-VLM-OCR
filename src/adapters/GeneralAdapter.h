#pragma once
#include "../core/ModelAdapter.h"
#include <QNetworkAccessManager>
#include <QMutex>
#include <QThread>

// 通用 OpenAI 接口兼容适配器（支持 Vision）
class GeneralAdapter : public ModelAdapter {
    Q_OBJECT
public:
    explicit GeneralAdapter(const ModelConfig& config, QObject* parent = nullptr);
    ~GeneralAdapter() override;

    bool initialize() override;
    OCRResult recognize(const QImage& image, const QString& prompt = QString()) override;
    bool isInitialized() const override { return m_initialized; }

private:
    QString encodeImageToBase64(const QImage& image);
    QString callAPI(const QString& imageBase64, const QString& prompt, bool hasImage, QString& errorMsg);
    QString parseAPIResponse(const QByteArray& response, QString& errorMsg);

    bool m_initialized;
    QString m_modelName;
    QString m_apiKey;
    QString m_apiUrl;
    float m_temperature;

    QMutex m_mutex;
};