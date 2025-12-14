#pragma once
#include <QObject>
#include <QMap>
#include <QList>
#include "../core/ModelAdapter.h"

// 模型管理器
// 负责管理所有可用的模型适配器
class ModelManager : public QObject {
    Q_OBJECT
    
public:
    explicit ModelManager(QObject* parent = nullptr);
    ~ModelManager() override;
    
    // 添加模型
    bool addModel(ModelAdapter* adapter);
    
    // 移除模型
    void removeModel(const QString& modelId);
    
    // 获取模型
    ModelAdapter* getModel(const QString& modelId) const;
    
    // 获取所有模型列表
    QList<ModelAdapter*> getAllModels() const;
    
    // 获取已初始化的模型列表
    QList<ModelAdapter*> getInitializedModels() const;
    
    // 设置当前激活的模型
    void setActiveModel(const QString& modelId);
    
    // 获取当前激活的模型
    ModelAdapter* getActiveModel() const;
    
    // 初始化所有模型
    void initializeAll();
    
    // 从配置文件加载模型
    bool loadModelsFromConfig(const QString& configPath);
    
    // 保存模型配置
    bool saveModelsToConfig(const QString& configPath);
    
signals:
    // 模型添加
    void modelAdded(const QString& modelId);
    
    // 模型移除
    void modelRemoved(const QString& modelId);
    
    // 当前模型变化
    void activeModelChanged(const QString& modelId);
    
    // 模型初始化完成
    void modelInitialized(const QString& modelId, bool success);
    
private:
    QMap<QString, ModelAdapter*> m_models;
    QString m_activeModelId;
};