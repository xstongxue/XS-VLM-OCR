#pragma once
#include <QObject>
#include <QVector>
#include "../core/HistoryItem.h"

// 历史记录管理器
class HistoryManager: public QObject {
    Q_OBJECT  // 允许信号槽机制

public:
    explicit HistoryManager(QObject* parent = nullptr);
    ~HistoryManager() override;

    // 加载历史记录
    void loadHistory();
    
    // 保存历史记录
    void saveHistory();
    
    // 添加历史记录
    void addHistoryItem(const HistoryItem& item);

    // 清除历史记录
    void clearHistory();
    
    // 获取历史记录列表
    const QVector<HistoryItem>& getHistory() const;
    
    // 删除单条记录
    void removeHistoryItem(int index);

    // 设置是否启用持久化
    void setPersistenceEnabled(bool enabled);
    bool isPersistenceEnabled() const;

    // 设置最大历史记录数量
    void setMaxHistory(int max);
    int getMaxHistory() const;

signals:
    void historyChanged();

private:
    QVector<HistoryItem> m_history;
    QString m_historyDir;
    QString m_imagesDir;
    QString m_historyFile;
    bool m_persistenceEnabled = false;
    int m_maxHistory;          // 最大历史记录数量(通过设置控制)

    void ensureDirectories();  // 确保文件夹存在
};