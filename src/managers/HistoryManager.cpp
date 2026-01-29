#include "HistoryManager.h"
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QDateTime>

HistoryManager::HistoryManager(QObject* parent) : QObject(parent) {
    m_historyDir = QDir::currentPath() + "/history";
    m_imagesDir = m_historyDir + "/images";
    ensureDirectories();
}

HistoryManager::~HistoryManager() {
    // 不需要手动关闭 m_db，QSqlDatabase 会自动管理引用计数
    // 连接会在应用程序退出时由 Qt 清理
    QString connectionName;
    {
        QSqlDatabase db = QSqlDatabase::database();
        connectionName = db.connectionName();
    }
    // 注意：不能在还有打开的查询或数据库对象时 removeDatabase
    // 这里我们只是析构管理器，不需要强制移除连接
}

void HistoryManager::ensureDirectories() {
    QDir dir(m_historyDir);
    if (!dir.exists()) dir.mkpath(".");
    QDir imgDir(m_imagesDir);
    if (!imgDir.exists()) imgDir.mkpath(".");
}

QSqlDatabase HistoryManager::getDatabase() {
    QSqlDatabase db;
    if (QSqlDatabase::contains("qt_sql_default_connection")) {
        db = QSqlDatabase::database("qt_sql_default_connection");
    } else {
        db = QSqlDatabase::addDatabase("QSQLITE");
        db.setDatabaseName(m_historyDir + "/history.db");
    }
    
    if (!db.isOpen()) {
        if (!db.open()) {
            qCritical() << "HistoryManager: Failed to open database:" << db.lastError().text();
        } else {
            // 首次打开，确保表结构存在
            QSqlQuery query(db);
            bool success = query.exec(
                "CREATE TABLE IF NOT EXISTS history ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "timestamp INTEGER NOT NULL, "
                "image_path TEXT, "
                "source INTEGER, "
                "success INTEGER, "
                "full_text TEXT, "
                "model_name TEXT, "
                "processing_time_ms INTEGER, "
                "error_message TEXT"
                ")"
            );
            if (!success) {
                qCritical() << "HistoryManager: Failed to create table:" << query.lastError().text();
            } else {
                query.exec("CREATE INDEX IF NOT EXISTS idx_timestamp ON history(timestamp DESC)");
            }
        }
    }
    return db;
}

void HistoryManager::loadHistory() {
    QSqlDatabase db = getDatabase();
    if (!db.isOpen()) return;
    
    // 从数据库加载到内存缓存
    m_history.clear();
    
    // 限制加载数量
    QSqlQuery query(db);
    query.prepare("SELECT * FROM history ORDER BY timestamp DESC LIMIT :limit");
    query.bindValue(":limit", m_maxHistory > 0 ? m_maxHistory : 100);
    
    if (query.exec()) {
        while (query.next()) {
            HistoryItem item;
            item.timestamp = QDateTime::fromMSecsSinceEpoch(query.value("timestamp").toLongLong());
            item.imagePath = query.value("image_path").toString();
            item.source = static_cast<SubmitSource>(query.value("source").toInt());
            
            item.result.success = query.value("success").toBool();
            item.result.fullText = query.value("full_text").toString();
            item.result.modelName = query.value("model_name").toString();
            item.result.processingTimeMs = query.value("processing_time_ms").toLongLong();
            item.result.errorMessage = query.value("error_message").toString();
            
            if (!item.imagePath.isEmpty() && QFile::exists(item.imagePath)) {
                item.image.load(item.imagePath);
            }
            
            m_history.append(item);
        }
    } else {
        qWarning() << "HistoryManager: Load failed:" << query.lastError().text();
    }

    emit historyChanged();
}

void HistoryManager::addHistoryItem(const HistoryItem& item) {
    HistoryItem newItem = item;

    // 1. 保存图片到磁盘 (仅在启用持久化时)
    if (m_persistenceEnabled && newItem.imagePath.isEmpty()) {
        QString fileName = QString("%1.png").arg(newItem.timestamp.toString("yyyyMMdd_HHmmss_zzz"));
        QString filePath = m_imagesDir + "/" + fileName;
        
        if (newItem.image.save(filePath)) {
            newItem.imagePath = filePath;
        } else {
            qWarning() << "HistoryManager: Failed to save image:" << filePath;
        }
    }
    
    // 2. 更新内存缓存
    m_history.prepend(newItem);
    
    // 3. 写入数据库 (仅在启用持久化时)
    if (m_persistenceEnabled) {
        QSqlDatabase db = getDatabase();
        if (db.isOpen()) {
            QSqlQuery query(db);
            query.prepare("INSERT INTO history (timestamp, image_path, source, success, full_text, model_name, processing_time_ms, error_message) "
                          "VALUES (:ts, :path, :src, :success, :txt, :model, :time, :err)");
            
            query.bindValue(":ts", newItem.timestamp.toMSecsSinceEpoch());
            query.bindValue(":path", newItem.imagePath);
            query.bindValue(":src", static_cast<int>(newItem.source));
            query.bindValue(":success", newItem.result.success ? 1 : 0);
            query.bindValue(":txt", newItem.result.fullText);
            query.bindValue(":model", newItem.result.modelName);
            query.bindValue(":time", newItem.result.processingTimeMs);
            query.bindValue(":err", newItem.result.errorMessage);
            
            if (!query.exec()) {
                 qWarning() << "HistoryManager: Insert failed:" << query.lastError().text();
            }
        }
    }

    // 4. 检查数量限制
    enforceMaxHistory();
    
    emit historyChanged();
}

void HistoryManager::enforceMaxHistory() {
    if (m_maxHistory <= 0) return;
    
    // 内存清理
    if (m_history.size() > m_maxHistory) {
        m_history.resize(m_maxHistory);
    }
    
    if (!m_persistenceEnabled) return;

    QSqlDatabase db = getDatabase();
    if (!db.isOpen()) return;

    // 数据库清理：保留最新的 N 条，删除其余的
    QSqlQuery query(db);
    query.prepare("SELECT image_path FROM history ORDER BY timestamp DESC LIMIT -1 OFFSET :offset");
    query.bindValue(":offset", m_maxHistory);
    
    // 先获取要删除的图片路径，以便清理磁盘文件
    if (query.exec()) {
        while (query.next()) {
            QString path = query.value(0).toString();
            if (!path.isEmpty() && QFile::exists(path)) {
                QFile::remove(path);
            }
        }
    }
    
    // 执行数据库删除
    QSqlQuery delQuery(db);
    delQuery.prepare("DELETE FROM history WHERE id NOT IN (SELECT id FROM history ORDER BY timestamp DESC LIMIT :limit)");
    delQuery.bindValue(":limit", m_maxHistory);
    delQuery.exec();
}

const QVector<HistoryItem>& HistoryManager::getHistory() const {
    return m_history;
}

void HistoryManager::clearHistory() {
    QSqlDatabase db = getDatabase();

    // 1. 清理磁盘图片
    if (m_persistenceEnabled && db.isOpen()) {
        QSqlQuery query(db);
        query.exec("SELECT image_path FROM history");
        while (query.next()) {
            QString path = query.value(0).toString();
            if (!path.isEmpty() && QFile::exists(path)) {
                QFile::remove(path);
            }
        }
    } else {
        // 如果未启用持久化，尝试清理缓存列表中的图片
        for (const auto& item : m_history) {
            if (!item.imagePath.isEmpty() && QFile::exists(item.imagePath)) {
                QFile::remove(item.imagePath);
            }
        }
    }

    // 2. 清空数据库
    if (db.isOpen()) {
        QSqlQuery query(db);
        query.exec("DELETE FROM history");
        query.exec("VACUUM"); // 释放空间
    }
    
    // 3. 清空内存
    m_history.clear();
    
    emit historyChanged();
}

void HistoryManager::removeHistoryItem(int index) {
    if (index < 0 || index >= m_history.size()) return;

    const HistoryItem& item = m_history[index];
    
    if (m_persistenceEnabled) {
        // 1. 删除文件
        if (!item.imagePath.isEmpty() && QFile::exists(item.imagePath)) {
            QFile::remove(item.imagePath);
        }
        
        // 2. 删除数据库记录
        QSqlDatabase db = getDatabase();
        if (db.isOpen()) {
            QSqlQuery query(db);
            query.prepare("DELETE FROM history WHERE timestamp = :ts");
            query.bindValue(":ts", item.timestamp.toMSecsSinceEpoch());
            query.exec();
        }
    }
    
    // 3. 删除内存记录
    m_history.remove(index);
    
    emit historyChanged();
}

void HistoryManager::setPersistenceEnabled(bool enabled) {
    qDebug() << "HistoryManager: Setting persistence to" << enabled;
    if (m_persistenceEnabled == enabled) return;
    m_persistenceEnabled = enabled;
    
    if (enabled) {
        // 获取数据库连接
        QSqlDatabase db = getDatabase();
        
        if (!db.isOpen()) {
            qCritical() << "HistoryManager: Failed to open database, cannot enable persistence.";
            m_persistenceEnabled = false; // 回滚状态
            return;
        }

        qDebug() << "HistoryManager: Persistence enabled, migrating" << m_history.size() << "items to DB.";
        
        db.transaction();
        QSqlQuery insertQuery(db);
        insertQuery.prepare("INSERT INTO history (timestamp, image_path, source, success, full_text, model_name, processing_time_ms, error_message) "
                      "VALUES (:ts, :path, :src, :success, :txt, :model, :time, :err)");

        // 将内存中的历史记录保存到数据库
        for (auto& item : m_history) {
             // 如果已经持久化，跳过
             if (item.persisted) continue;

             if (item.imagePath.isEmpty() && !item.image.isNull()) {
                 QString fileName = QString("%1.png").arg(item.timestamp.toString("yyyyMMdd_HHmmss_zzz"));
                 QString filePath = m_imagesDir + "/" + fileName;
                 if (item.image.save(filePath)) {
                     item.imagePath = filePath;
                 } else {
                     qWarning() << "HistoryManager: Failed to save image during migration:" << filePath;
                 }
             }
             
             insertQuery.bindValue(":ts", item.timestamp.toMSecsSinceEpoch());
             insertQuery.bindValue(":path", item.imagePath);
             insertQuery.bindValue(":src", static_cast<int>(item.source));
             insertQuery.bindValue(":success", item.result.success ? 1 : 0);
             insertQuery.bindValue(":txt", item.result.fullText);
             insertQuery.bindValue(":model", item.result.modelName);
             insertQuery.bindValue(":time", item.result.processingTimeMs);
             insertQuery.bindValue(":err", item.result.errorMessage);
             
             if (!insertQuery.exec()) {
                qWarning() << "HistoryManager: Failed to insert item during migration:" << insertQuery.lastError().text();
             } else {
                item.persisted = true;
             }
        }
        db.commit();
        qDebug() << "HistoryManager: Migration completed.";
    }
}

bool HistoryManager::isPersistenceEnabled() const {
    return m_persistenceEnabled;
}

void HistoryManager::setMaxHistory(int max) {
    if (max <= 0) return;
    m_maxHistory = max;
    enforceMaxHistory();
}

int HistoryManager::getMaxHistory() const {
    return m_maxHistory;
}