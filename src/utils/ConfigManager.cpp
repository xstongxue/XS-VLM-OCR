#include "ConfigManager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QDir>

ConfigManager::ConfigManager(QObject* parent)
    : QObject(parent)
{
}

bool ConfigManager::loadConfig(const QString& configPath)
{
    m_configPath = configPath;
    
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString error = QString("无法打开配置文件: %1").arg(configPath);
        qWarning() << error;
        emit configError(error);
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        QString error = QString("JSON 解析错误: %1").arg(parseError.errorString());
        qWarning() << error;
        emit configError(error);
        return false;
    }
    
    if (!doc.isObject()) {
        QString error = "配置文件格式错误: 根元素不是对象";
        qWarning() << error;
        emit configError(error);
        return false;
    }
    
    QJsonObject root = doc.object();
    
    // 解析提供商配置
    m_providers.clear();
    if (root.contains("providers") && root["providers"].isObject()) {
        QJsonObject providers = root["providers"].toObject();
        for (auto it = providers.begin(); it != providers.end(); ++it) {
            QString providerId = it.key();
            if (it.value().isObject()) {
                ProviderConfig provider = parseProviderConfig(providerId, it.value().toObject());
                m_providers[providerId] = provider;
            }
        }
    }
    
    // 解析模型配置
    m_modelConfigs.clear();
    if (root.contains("models") && root["models"].isArray()) {
        QJsonArray models = root["models"].toArray();
        for (const QJsonValue& value : models) {
            if (value.isObject()) {
                ModelConfig config = parseModelConfig(value.toObject());
                m_modelConfigs.append(config);
            }
        }
    }
    
    // 解析设置
    m_settings.clear();
    if (root.contains("settings") && root["settings"].isObject()) {
        QJsonObject settings = root["settings"].toObject();
        for (auto it = settings.begin(); it != settings.end(); ++it) {
            QString key = it.key();
            QJsonValue value = it.value();
            
            if (value.isBool()) {
                m_settings[key] = value.toBool();
            } else if (value.isDouble()) {
                m_settings[key] = value.toDouble();
            } else if (value.isString()) {
                m_settings[key] = value.toString();
            }
        }
    }
    
    // 解析提示词模板
    m_promptTemplates.clear();
    if (root.contains("prompt_templates") && root["prompt_templates"].isArray()) {
        QJsonArray templates = root["prompt_templates"].toArray();
        for (const QJsonValue& value : templates) {
            if (value.isObject()) {
                PromptTemplate tmpl = parsePromptTemplate(value.toObject());
                m_promptTemplates.append(tmpl);
            }
        }
    }
    
    qDebug() << "ConfigManager: Loaded" << m_providers.size() << "providers";
    qDebug() << "ConfigManager: Loaded" << m_modelConfigs.size() << "model configs";
    qDebug() << "ConfigManager: Loaded" << m_promptTemplates.size() << "prompt templates";
    qDebug() << "ConfigManager: Loaded" << m_settings.size() << "settings";
    
    emit configLoaded();
    return true;
}

bool ConfigManager::saveConfig(const QString& configPath)
{
    QJsonObject root;
    
    // 保存提供商配置
    QJsonObject providersObj;
    for (auto it = m_providers.begin(); it != m_providers.end(); ++it) {
        providersObj[it.key()] = toJsonObject(it.value());
    }
    root["providers"] = providersObj;
    
    // 保存模型配置
    QJsonArray modelsArray;
    for (const ModelConfig& config : m_modelConfigs) {
        modelsArray.append(toJsonObject(config));
    }
    root["models"] = modelsArray;
    
    // 保存提示词模板
    QJsonArray templatesArray;
    for (const PromptTemplate& tmpl : m_promptTemplates) {
        templatesArray.append(toJsonObject(tmpl));
    }
    root["prompt_templates"] = templatesArray;
    
    // 保存设置
    QJsonObject settingsObj;
    for (auto it = m_settings.begin(); it != m_settings.end(); ++it) {
        QString key = it.key();
        QVariant value = it.value();
        
        if (value.type() == QVariant::Bool) {
            settingsObj[key] = value.toBool();
        } else if (value.type() == QVariant::Double || value.type() == QVariant::Int) {
            settingsObj[key] = value.toDouble();
        } else {
            settingsObj[key] = value.toString();
        }
    }
    root["settings"] = settingsObj;
    
    // 写入文件
    QJsonDocument doc(root);
    QByteArray data = doc.toJson(QJsonDocument::Indented);
    
    QFile file(configPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QString error = QString("无法写入配置文件: %1").arg(configPath);
        qWarning() << error;
        emit configError(error);
        return false;
    }
    
    file.write(data);
    file.close();
    
    qDebug() << "ConfigManager: Configuration saved to" << configPath;
    emit configSaved();
    return true;
}

ModelConfig ConfigManager::parseModelConfig(const QJsonObject& obj)
{
    ModelConfig config;
    
    config.id = obj["id"].toString();
    config.displayName = obj["displayName"].toString();
    config.type = obj["type"].toString();
    config.engine = obj["engine"].toString();
    config.enabled = obj.value("enabled").toBool(true);
    config.provider = obj.value("provider").toString();
    
    // 解析参数
    if (obj.contains("params") && obj["params"].isObject()) {
        QJsonObject params = obj["params"].toObject();
        for (auto it = params.begin(); it != params.end(); ++it) {
            config.params[it.key()] = it.value().toString();
        }
    }
    
    // 如果模型使用 provider，从 provider 继承 API key 和 host
    if (!config.provider.isEmpty() && m_providers.contains(config.provider)) {
        ProviderConfig provider = m_providers[config.provider];
        
        qDebug() << "  → 模型" << config.id << "使用提供商:" << provider.name;
        qDebug() << "     API Key:" << (provider.apiKey.isEmpty() ? "未设置" : "已设置");
        qDebug() << "     API Host:" << provider.apiHost;
        
        // 从 provider 继承 API key 和 host（如果模型参数中没有指定）
        if (!config.params.contains("api_key") || config.params["api_key"].isEmpty()) {
            config.params["api_key"] = provider.apiKey;
        }
        if (!config.params.contains("api_host") || config.params["api_host"].isEmpty()) {
            config.params["api_host"] = provider.apiHost;
        }
    }
    
    return config;
}

QJsonObject ConfigManager::toJsonObject(const ModelConfig& config)
{
    QJsonObject obj;
    
    obj["id"] = config.id;
    obj["displayName"] = config.displayName;
    obj["type"] = config.type;
    obj["engine"] = config.engine;
    obj["enabled"] = config.enabled;
    
    // 保存 provider
    if (!config.provider.isEmpty()) {
        obj["provider"] = config.provider;
    }
    
    // 转换参数
    QJsonObject params;
    for (auto it = config.params.begin(); it != config.params.end(); ++it) {
        params[it.key()] = it.value();
    }
    obj["params"] = params;
    
    return obj;
}

QList<ModelConfig> ConfigManager::getModelConfigs() const
{
    return m_modelConfigs;
}

QVariant ConfigManager::getSetting(const QString& key, const QVariant& defaultValue) const
{
    return m_settings.value(key, defaultValue);
}

void ConfigManager::setSetting(const QString& key, const QVariant& value)
{
    m_settings[key] = value;
}

void ConfigManager::updateModelConfig(const QString& modelId, const ModelConfig& config)
{
    for (int i = 0; i < m_modelConfigs.size(); ++i) {
        if (m_modelConfigs[i].id == modelId) {
            m_modelConfigs[i] = config;
            qDebug() << "ConfigManager: 更新模型配置:" << modelId;
            return;
        }
    }
    
    // 如果不存在，添加新配置
    m_modelConfigs.append(config);
    qDebug() << "ConfigManager: 添加新模型配置:" << modelId;
}

void ConfigManager::removeModelConfig(const QString& modelId)
{
    for (int i = 0; i < m_modelConfigs.size(); ++i) {
        if (m_modelConfigs[i].id == modelId) {
            m_modelConfigs.removeAt(i);
            qDebug() << "ConfigManager: 删除模型配置:" << modelId;
            return;
        }
    }
    qWarning() << "ConfigManager: 未找到要删除的模型:" << modelId;
}

QList<PromptTemplate> ConfigManager::getPromptTemplates() const
{
    return m_promptTemplates;
}

void ConfigManager::addPromptTemplate(const PromptTemplate& tmpl)
{
    m_promptTemplates.append(tmpl);
    qDebug() << "ConfigManager: 添加提示词模板:" << tmpl.name;
}

void ConfigManager::updatePromptTemplate(int index, const PromptTemplate& tmpl)
{
    if (index >= 0 && index < m_promptTemplates.size()) {
        m_promptTemplates[index] = tmpl;
        qDebug() << "ConfigManager: 更新提示词模板:" << tmpl.name << "索引:" << index;
    } else {
        qWarning() << "ConfigManager: 无效的模板索引:" << index;
    }
}

void ConfigManager::removePromptTemplate(int index)
{
    if (index >= 0 && index < m_promptTemplates.size()) {
        QString name = m_promptTemplates[index].name;
        m_promptTemplates.removeAt(index);
        qDebug() << "ConfigManager: 删除提示词模板:" << name << "索引:" << index;
    } else {
        qWarning() << "ConfigManager: 无效的模板索引:" << index;
    }
}

PromptTemplate ConfigManager::parsePromptTemplate(const QJsonObject& obj)
{
    PromptTemplate tmpl;
    tmpl.name = obj["name"].toString();
    tmpl.content = obj["content"].toString();
    tmpl.type = obj.value("type").toString("识别");  // 默认为"识别"
    tmpl.category = obj.value("category").toString("通用");
    return tmpl;
}

QJsonObject ConfigManager::toJsonObject(const PromptTemplate& tmpl)
{
    QJsonObject obj;
    obj["name"] = tmpl.name;
    obj["content"] = tmpl.content;
    obj["type"] = tmpl.type;
    obj["category"] = tmpl.category;
    return obj;
}

ProviderConfig ConfigManager::parseProviderConfig(const QString& id, const QJsonObject& obj)
{
    ProviderConfig provider;
    provider.id = id;
    provider.name = obj["name"].toString();
    provider.apiKey = obj["api_key"].toString();
    provider.apiHost = obj["api_host"].toString();
    provider.description = obj.value("description").toString();
    return provider;
}

QJsonObject ConfigManager::toJsonObject(const ProviderConfig& provider)
{
    QJsonObject obj;
    obj["name"] = provider.name;
    obj["api_key"] = provider.apiKey;
    obj["api_host"] = provider.apiHost;
    if (!provider.description.isEmpty()) {
        obj["description"] = provider.description;
    }
    return obj;
}

QMap<QString, ProviderConfig> ConfigManager::getProviders() const
{
    return m_providers;
}

void ConfigManager::updateProvider(const QString& providerId, const ProviderConfig& config)
{
    m_providers[providerId] = config;
    qDebug() << "ConfigManager: 更新提供商配置:" << providerId;
}