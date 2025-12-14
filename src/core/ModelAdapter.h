#pragma once
#include <QObject>
#include <QImage>
#include <QString>
#include <QMap>
#include "OCRResult.h"

// 模型配置
struct ModelConfig {
    QString id;              // 模型唯一标识
    QString displayName;     // 显示名称
    QString type;            // "local" 或 "online"
    QString engine;          // 引擎类型 (tesseract, paddle, custom)
    QString provider;        // 提供商ID (aliyun, 空表示不使用)
    QMap<QString, QString> params;  // 参数配置
    bool enabled;            // 是否启用
    
    ModelConfig() : enabled(true) {}
};

// 模型适配器抽象基类
class ModelAdapter : public QObject {
    Q_OBJECT
    
public:
    explicit ModelAdapter(const ModelConfig& config, QObject* parent = nullptr)
        : QObject(parent), m_config(config) {}
    
    virtual ~ModelAdapter() = default;
    
    // 初始化模型（加载模型文件等）
    virtual bool initialize() = 0;
    
    // 识别图像
    virtual OCRResult recognize(const QImage& image, const QString& prompt = QString()) = 0;
    
    // 是否已初始化
    virtual bool isInitialized() const = 0;
    
    // 获取模型配置
    const ModelConfig& config() const { return m_config; }
    
    // 获取模型类型描述
    virtual QString typeDescription() const {
        return m_config.type == "local" ? "[离线]" : "[在线]";
    }
    
protected:
    ModelConfig m_config;
};