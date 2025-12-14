#include "ModelManager.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>

ModelManager::ModelManager(QObject* parent)
    : QObject(parent)
{
}

ModelManager::~ModelManager()
{
    // 清理所有模型
    qDeleteAll(m_models);
    m_models.clear();
}

bool ModelManager::addModel(ModelAdapter* adapter)
{
    if (!adapter) {
        qWarning() << "ModelManager: 无法添加空适配器";
        return false;
    }
    
    QString id = adapter->config().id;
    
    if (m_models.contains(id)) {
        qWarning() << "ModelManager: 模型已存在:" << id;
        return false;
    }
    
    m_models.insert(id, adapter);
    emit modelAdded(id);
    
    qDebug() << "ModelManager: 模型已添加:" << id << adapter->config().displayName;
    
    // 如果这是第一个模型，自动设为激活
    if (m_models.size() == 1) {
        setActiveModel(id);
    }
    
    return true;
}

void ModelManager::removeModel(const QString& modelId)
{
    if (!m_models.contains(modelId)) {
        qWarning() << "ModelManager: 模型未找到:" << modelId;
        return;
    }
    
    ModelAdapter* adapter = m_models.take(modelId);
    delete adapter;
    
    emit modelRemoved(modelId);
    
    // 如果移除的是当前激活模型，切换到其他模型
    if (m_activeModelId == modelId) {
        if (!m_models.isEmpty()) {
            setActiveModel(m_models.firstKey());
        } else {
            m_activeModelId.clear();
            emit activeModelChanged(QString());
        }
    }
    
    qDebug() << "ModelManager: 模型已移除:" << modelId;
}

ModelAdapter* ModelManager::getModel(const QString& modelId) const
{
    return m_models.value(modelId, nullptr);
}

QList<ModelAdapter*> ModelManager::getAllModels() const
{
    return m_models.values();
}

QList<ModelAdapter*> ModelManager::getInitializedModels() const
{
    QList<ModelAdapter*> initialized;
    for (ModelAdapter* adapter : m_models.values()) {
        if (adapter && adapter->isInitialized()) {
            initialized.append(adapter);
        }
    }
    return initialized;
}

void ModelManager::setActiveModel(const QString& modelId)
{
    if (!m_models.contains(modelId)) {
        qWarning() << "ModelManager: 无法设置激活模型,未找到:" << modelId;
        return;
    }
    
    if (m_activeModelId == modelId) {
        return; // 已经是当前模型
    }
    
    m_activeModelId = modelId;
    emit activeModelChanged(modelId);
    
    ModelAdapter* adapter = m_models[modelId];
    qDebug() << "ModelManager: 激活模型已更改为:" 
             << adapter->config().displayName;
}

ModelAdapter* ModelManager::getActiveModel() const
{
    return m_models.value(m_activeModelId, nullptr);
}

void ModelManager::initializeAll()
{
    qDebug() << "ModelManager: 初始化所有模型...";
    
    for (auto it = m_models.begin(); it != m_models.end(); ++it) {
        QString id = it.key();
        ModelAdapter* adapter = it.value();
        
        if (adapter->isInitialized()) {
            qDebug() << "ModelManager: 模型已初始化:" << id;
            continue;
        }
        
        bool success = adapter->initialize();
        emit modelInitialized(id, success);
        
        if (success) {
            qDebug() << "ModelManager: 模型初始化成功:" << id;
        } else {
            qWarning() << "ModelManager: 模型初始化失败:" << id;
        }
    }
}