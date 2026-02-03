#pragma once
#include <QObject>
#include <QVector>
#include <QSqlDatabase>
#include <QMap>
#include "../core/HistoryItem.h"

// 历史记录管理器
class HistoryManager: public QObject {
    Q_OBJECT

public:
    explicit HistoryManager(QObject* parent = nullptr);
    ~HistoryManager() override;

    // 加载历史记录 (初始化数据库)
    void loadHistory();
    
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

    // 计算内容哈希
    static QString computeContentHash(const QImage& img, const QString& prompt, const QString& model, const QMap<QString, QString>& params = QMap<QString, QString>());

    // 根据哈希查找历史记录
    HistoryItem findItemByHash(const QString& hash);

signals:
    void historyChanged();

private:
    QVector<HistoryItem> m_history; // 内存缓存，用于 UI 显示
    QString m_historyDir;
    QString m_imagesDir;
    
    bool m_persistenceEnabled = false;
    int m_maxHistory;

    void ensureDirectories();
    QSqlDatabase getDatabase(); // 获取数据库连接
    void enforceMaxHistory();   // 强制执行数量限制
};