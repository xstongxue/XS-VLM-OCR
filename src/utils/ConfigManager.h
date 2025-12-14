#pragma once
#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariant>
#include <QMap>
#include <QList>
#include "../core/ModelAdapter.h"

// 提供商配置
struct ProviderConfig {
    QString id;          // 提供商ID (如: aliyun, openai)
    QString name;        // 显示名称
    QString apiKey;      // API密钥
    QString apiHost;     // API地址
    QString description; // 描述
    
    ProviderConfig() {}
};

// 提示词模板
struct PromptTemplate {
    QString name;
    QString content;
    QString type;      // 主分类：识别、翻译、解答
    QString category;  // 子分类：通用、表格、公式等
    
    PromptTemplate() {}
    PromptTemplate(const QString& n, const QString& c, const QString& t = "识别", const QString& cat = "通用")
        : name(n), content(c), type(t), category(cat) {}
};

// 配置管理器
// 负责加载和保存模型配置
class ConfigManager : public QObject {
    Q_OBJECT
    
public:
    explicit ConfigManager(QObject* parent = nullptr);
    ~ConfigManager() override = default;
    
    // 加载配置文件
    bool loadConfig(const QString& configPath);
    
    // 保存配置文件
    bool saveConfig(const QString& configPath);
    
    // 获取模型配置列表
    QList<ModelConfig> getModelConfigs() const;
    
    // 获取提供商配置列表
    QMap<QString, ProviderConfig> getProviders() const;
    
    // 更新提供商配置
    void updateProvider(const QString& providerId, const ProviderConfig& config);
    
    // 获取提示词模板列表
    QList<PromptTemplate> getPromptTemplates() const;
    
    // 添加提示词模板
    void addPromptTemplate(const PromptTemplate& tmpl);
    
    // 更新提示词模板
    void updatePromptTemplate(int index, const PromptTemplate& tmpl);
    
    // 删除提示词模板
    void removePromptTemplate(int index);
    
    // 获取设置项
    QVariant getSetting(const QString& key, const QVariant& defaultValue = QVariant()) const;
    
    // 设置设置项
    void setSetting(const QString& key, const QVariant& value);
    
    // 更新模型配置
    void updateModelConfig(const QString& modelId, const ModelConfig& config);
    
    // 删除模型配置
    void removeModelConfig(const QString& modelId);
    
    // 获取配置文件路径
    QString getConfigPath() const { return m_configPath; }
    
signals:
    void configLoaded();
    void configSaved();
    void configError(const QString& error);
    
private:
    // 解析 JSON 为 ModelConfig
    ModelConfig parseModelConfig(const QJsonObject& obj);
    
    // 将 ModelConfig 转换为 JSON
    QJsonObject toJsonObject(const ModelConfig& config);
    
    // 解析提示词模板
    PromptTemplate parsePromptTemplate(const QJsonObject& obj);
    
    // 提示词模板转 JSON
    QJsonObject toJsonObject(const PromptTemplate& tmpl);
    
    // 解析提供商配置
    ProviderConfig parseProviderConfig(const QString& id, const QJsonObject& obj);
    
    // 提供商配置转 JSON
    QJsonObject toJsonObject(const ProviderConfig& provider);
    
    QString m_configPath;
    QList<ModelConfig> m_modelConfigs;
    QList<PromptTemplate> m_promptTemplates;
    QMap<QString, ProviderConfig> m_providers;  // 提供商配置
    QMap<QString, QVariant> m_settings;
};