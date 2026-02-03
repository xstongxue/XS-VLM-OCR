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
    

    // 设置是否启用持久化
    void setPersistenceEnabled(bool enabled);
    bool isPersistenceEnabled() const;

    // 设置最大历史记录数量
    void setMaxHistory(int max);
    int getMaxHistory() const;

    struct HistoryFilter {
        QDateTime startTime;
        QDateTime endTime;
        QString keyword;
    };

    // 获取历史记录总数 (用于分页)
    int getTotalCount(const HistoryFilter& filter = HistoryFilter());

    // 分页获取历史记录
    QVector<HistoryItem> getHistoryList(int page, int pageSize, const HistoryFilter& filter = HistoryFilter());

    // 根据ID获取单条详情 (包含加载图片)
    HistoryItem getHistoryDetail(long long id);

    // 计算内容哈希
    static QString computeContentHash(const QImage& img, const QString& prompt, const QString& model, const QMap<QString, QString>& params = QMap<QString, QString>());

    // 根据哈希查找历史记录
    HistoryItem findItemByHash(const QString& hash);

signals:
    void historyChanged();

private:
    QVector<HistoryItem> m_memoryHistory; // 内存缓存 (无论是否持久化都维护，用于缓存命中和非持久化模式)
    QString m_historyDir;
    QString m_imagesDir;
    
    bool m_persistenceEnabled = false;
    int m_maxHistory;

    void ensureDirectories();
    QSqlDatabase getDatabase(); // 获取数据库连接
    void enforceMaxHistory();   // 强制执行数量限制
};