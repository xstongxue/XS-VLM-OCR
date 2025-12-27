#pragma once
#include "../core/ModelAdapter.h"
#include <QNetworkAccessManager>
#include <QMutex>
#include <QThread>

// 字节豆包 Ark API 适配器
class DoubaoAdapter : public ModelAdapter {
    Q_OBJECT
public:
    explicit DoubaoAdapter(const ModelConfig& config, QObject* parent = nullptr);
    ~DoubaoAdapter() override = default;

    bool initialize() override;
    OCRResult recognize(const QImage& image, const QString& prompt = QString()) override;
    bool isInitialized() const override { return m_initialized; }

private:
    QString encodeImageToBase64(const QImage& image) const;
    QString callAPI(const QString& imageBase64, const QString& prompt, bool hasImage, QString& errorMsg);
    QString parseResponse(const QByteArray& response, QString& errorMsg) const;

    bool m_initialized;
    QString m_modelName;
    QString m_apiKey;
    QString m_apiUrl;
    float m_temperature;

    QMutex m_mutex;
};
