#include "HistoryManager.h"
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QDateTime>
#include <QCryptographicHash>
#include <QBuffer>

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
                "error_message TEXT, "
                "content_hash TEXT"
                ")"
            );
            if (!success) {
                qCritical() << "HistoryManager: Failed to create table:" << query.lastError().text();
            } else {
                // 检查 content_hash 列是否存在 (用于旧版本升级)
                if (!db.record("history").contains("content_hash")) {
                    qDebug() << "HistoryManager: Upgrading database schema (adding content_hash)";
                    query.exec("ALTER TABLE history ADD COLUMN content_hash TEXT");
                    query.exec("CREATE INDEX IF NOT EXISTS idx_content_hash ON history(content_hash)");
                }

                query.exec("CREATE INDEX IF NOT EXISTS idx_timestamp ON history(timestamp DESC)");
                query.exec("CREATE INDEX IF NOT EXISTS idx_content_hash ON history(content_hash)");
            }
        }
    }
    return db;
}

void HistoryManager::loadHistory() {
    // 如果开启了持久化，从数据库预加载最近的记录到内存缓存，以便加速缓存命中
    if (m_persistenceEnabled) {
        m_memoryHistory.clear();
        QSqlDatabase db = getDatabase();
        if (db.isOpen()) {
            QSqlQuery query(db);
            query.prepare("SELECT * FROM history ORDER BY timestamp DESC LIMIT :limit");
            // 内存缓存限制为最大历史记录数，或者默认50条
            query.bindValue(":limit", m_maxHistory > 0 ? m_maxHistory : 50);
            if (query.exec()) {
                while (query.next()) {
                    HistoryItem item;
                    item.id = query.value("id").toLongLong();
                    item.timestamp = QDateTime::fromMSecsSinceEpoch(query.value("timestamp").toLongLong());
                    item.imagePath = query.value("image_path").toString();
                    item.source = static_cast<SubmitSource>(query.value("source").toInt());
                    item.result.success = query.value("success").toBool();
                    item.result.fullText = query.value("full_text").toString();
                    item.result.modelName = query.value("model_name").toString();
                    item.result.processingTimeMs = query.value("processing_time_ms").toLongLong();
                    item.result.errorMessage = query.value("error_message").toString();
                    item.contentHash = query.value("content_hash").toString();
                    // 内存缓存暂不加载图片数据以节省内存
                    m_memoryHistory.append(item);
                }
            }
        }
    }
    
    emit historyChanged();
}

int HistoryManager::getTotalCount(const HistoryFilter& filter) {
    if (!m_persistenceEnabled) {
        // 未开启持久化，从内存统计
        int count = 0;
        for (const auto& item : m_memoryHistory) {
            bool match = true;
            if (filter.startTime.isValid() && item.timestamp < filter.startTime) match = false;
            if (filter.endTime.isValid() && item.timestamp > filter.endTime) match = false;
            if (!filter.keyword.isEmpty() && !item.result.fullText.contains(filter.keyword, Qt::CaseInsensitive) 
                && !item.result.modelName.contains(filter.keyword, Qt::CaseInsensitive)) match = false;
            if (match) count++;
        }
        return count;
    }

    QSqlDatabase db = getDatabase();
    if (!db.isOpen()) return 0;

    QString sql = "SELECT COUNT(*) FROM history WHERE 1=1";
    if (filter.startTime.isValid()) sql += " AND timestamp >= :start";
    if (filter.endTime.isValid()) sql += " AND timestamp <= :end";
    if (!filter.keyword.isEmpty()) sql += " AND (full_text LIKE :kw OR model_name LIKE :kw)";

    QSqlQuery query(db);
    query.prepare(sql);
    
    if (filter.startTime.isValid()) query.bindValue(":start", filter.startTime.toMSecsSinceEpoch());
    if (filter.endTime.isValid()) query.bindValue(":end", filter.endTime.toMSecsSinceEpoch());
    if (!filter.keyword.isEmpty()) query.bindValue(":kw", "%" + filter.keyword + "%");

    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

QVector<HistoryItem> HistoryManager::getHistoryList(int page, int pageSize, const HistoryFilter& filter) {
    QVector<HistoryItem> list;
    
    if (!m_persistenceEnabled) {
        // 未开启持久化，从内存获取
        int offset = (page - 1) * pageSize;
        int count = 0;
        int added = 0;
        
        for (const auto& item : m_memoryHistory) {
            bool match = true;
            if (filter.startTime.isValid() && item.timestamp < filter.startTime) match = false;
            if (filter.endTime.isValid() && item.timestamp > filter.endTime) match = false;
            if (!filter.keyword.isEmpty() && !item.result.fullText.contains(filter.keyword, Qt::CaseInsensitive) 
                && !item.result.modelName.contains(filter.keyword, Qt::CaseInsensitive)) match = false;
            
            if (match) {
                if (count >= offset && added < pageSize) {
                    // 内存列表中的 item 可能没有加载图片，如果是为了列表显示，这正好。
                    // 详情显示会调用 getHistoryDetail
                    list.append(item);
                    added++;
                }
                count++;
                if (added >= pageSize) break;
            }
        }
        return list;
    }

    QSqlDatabase db = getDatabase();
    if (!db.isOpen()) return list;

    QString sql = "SELECT * FROM history WHERE 1=1";
    if (filter.startTime.isValid()) sql += " AND timestamp >= :start";
    if (filter.endTime.isValid()) sql += " AND timestamp <= :end";
    if (!filter.keyword.isEmpty()) sql += " AND (full_text LIKE :kw OR model_name LIKE :kw)";
    
    sql += " ORDER BY timestamp DESC LIMIT :limit OFFSET :offset";

    QSqlQuery query(db);
    query.prepare(sql);
    
    if (filter.startTime.isValid()) query.bindValue(":start", filter.startTime.toMSecsSinceEpoch());
    if (filter.endTime.isValid()) query.bindValue(":end", filter.endTime.toMSecsSinceEpoch());
    if (!filter.keyword.isEmpty()) query.bindValue(":kw", "%" + filter.keyword + "%");
    
    query.bindValue(":limit", pageSize);
    query.bindValue(":offset", (page - 1) * pageSize);

    if (query.exec()) {
        while (query.next()) {
            HistoryItem item;
            item.id = query.value("id").toLongLong();
            item.timestamp = QDateTime::fromMSecsSinceEpoch(query.value("timestamp").toLongLong());
            item.imagePath = query.value("image_path").toString();
            item.source = static_cast<SubmitSource>(query.value("source").toInt());
            
            item.result.success = query.value("success").toBool();
            // 列表模式只读取前100个字符作为预览，避免大量内存占用
            QString fullText = query.value("full_text").toString();
            item.result.fullText = fullText; 
            item.result.modelName = query.value("model_name").toString();
            item.result.processingTimeMs = query.value("processing_time_ms").toLongLong();
            item.result.errorMessage = query.value("error_message").toString();
            item.contentHash = query.value("content_hash").toString();
            
            // 注意：列表模式不加载图片 QImage，只保留路径
            // item.image.load(item.imagePath); 
            
            list.append(item);
        }
    } else {
        qWarning() << "HistoryManager: List query failed:" << query.lastError().text();
    }
    return list;
}

HistoryItem HistoryManager::getHistoryDetail(long long id) {
    if (!m_persistenceEnabled) {
        // 未开启持久化时从内存查找 (使用时间戳作为临时ID)
        for (auto& item : m_memoryHistory) {
            if (item.id == id) {
                 // 确保图片已加载
                 if (item.image.isNull() && !item.imagePath.isEmpty() && QFile::exists(item.imagePath)) {
                     item.image.load(item.imagePath);
                 }
                 return item;
            }
        }
        return HistoryItem();
    }

    HistoryItem item;
    QSqlDatabase db = getDatabase();
    if (!db.isOpen()) return item;

    QSqlQuery query(db);
    query.prepare("SELECT * FROM history WHERE id = :id");
    query.bindValue(":id", id);

    if (query.exec() && query.next()) {
        item.id = query.value("id").toLongLong();
        item.timestamp = QDateTime::fromMSecsSinceEpoch(query.value("timestamp").toLongLong());
        item.imagePath = query.value("image_path").toString();
        item.source = static_cast<SubmitSource>(query.value("source").toInt());
        
        item.result.success = query.value("success").toBool();
        item.result.fullText = query.value("full_text").toString();
        item.result.modelName = query.value("model_name").toString();
        item.result.processingTimeMs = query.value("processing_time_ms").toLongLong();
        item.result.errorMessage = query.value("error_message").toString();
        item.contentHash = query.value("content_hash").toString();
        
        // 详情模式加载图片
        if (!item.imagePath.isEmpty() && QFile::exists(item.imagePath)) {
            item.image.load(item.imagePath);
        }
    }
    return item;
}

void HistoryManager::addHistoryItem(const HistoryItem& item) {
    HistoryItem newItem = item;

    // 0. 生成临时 ID (如果未持久化，使用时间戳作为 ID)
    if (newItem.id == -1) {
        newItem.id = newItem.timestamp.toMSecsSinceEpoch(); 
    }

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
    
    // 2. 更新内存缓存以支持快速检索与展示 (通过 enforceMaxHistory 控制内存占用)
    m_memoryHistory.prepend(newItem);
    
    // 3. 写入数据库 (仅在启用持久化时)
    if (m_persistenceEnabled) {
        QSqlDatabase db = getDatabase();
        if (db.isOpen()) {
            QSqlQuery query(db);
            query.prepare("INSERT INTO history (timestamp, image_path, source, success, full_text, model_name, processing_time_ms, error_message, content_hash) "
                          "VALUES (:ts, :path, :src, :success, :txt, :model, :time, :err, :hash)");
            
            query.bindValue(":ts", newItem.timestamp.toMSecsSinceEpoch());
            query.bindValue(":path", newItem.imagePath);
            query.bindValue(":src", static_cast<int>(newItem.source));
            query.bindValue(":success", newItem.result.success ? 1 : 0);
            query.bindValue(":txt", newItem.result.fullText);
            query.bindValue(":model", newItem.result.modelName);
            query.bindValue(":time", newItem.result.processingTimeMs);
            query.bindValue(":err", newItem.result.errorMessage);
            query.bindValue(":hash", newItem.contentHash);
            
            if (!query.exec()) {
                 qWarning() << "HistoryManager: Insert failed:" << query.lastError().text();
            } else {
                newItem.id = query.lastInsertId().toLongLong();
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
    if (m_memoryHistory.size() > m_maxHistory) {
        m_memoryHistory.resize(m_maxHistory);
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
    // 兼容旧接口，返回空列表，或者如果需要可以返回第一页
    // 由于我们移除了 m_history，这里返回临时空对象
    static QVector<HistoryItem> empty;
    return empty; 
}

void HistoryManager::clearHistory() {
    QSqlDatabase db = getDatabase();

    // 1. 清理磁盘图片
    if (db.isOpen()) {
        QSqlQuery query(db);
        query.exec("SELECT image_path FROM history");
        while (query.next()) {
            QString path = query.value(0).toString();
            if (!path.isEmpty() && QFile::exists(path)) {
                QFile::remove(path);
            }
        }
    } 
    
    // 2. 清空内存
    m_memoryHistory.clear();

    // 3. 清空数据库
    if (db.isOpen()) {
        QSqlQuery query(db);
        query.exec("DELETE FROM history");
        query.exec("VACUUM"); // 释放空间
    }
    
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

        qDebug() << "HistoryManager: Persistence enabled.";
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

QString HistoryManager::computeContentHash(const QImage& img, const QString& prompt, const QString& model, const QMap<QString, QString>& params) {
    if (img.isNull()) return QString();
    
    QCryptographicHash hasher(QCryptographicHash::Md5);
    
    // Hash Image Data
    QByteArray imgData;
    QBuffer buffer(&imgData);
    buffer.open(QIODevice::WriteOnly);
    // 使用 PNG 格式以确保无损和一致性
    img.save(&buffer, "PNG"); 
    hasher.addData(imgData);
    
    // Hash Prompt
    hasher.addData(prompt.toUtf8());
    
    // Hash Model
    hasher.addData(model.toUtf8());

    // Hash Params (QMap 参数按key排序)
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        // 跳过敏感信息 (API Key等) 避免因 Key 变更导致缓存失效
        if (it.key().compare("api_key", Qt::CaseInsensitive) == 0) continue;
        if (it.key().compare("secret_key", Qt::CaseInsensitive) == 0) continue;
        if (it.key().compare("access_token", Qt::CaseInsensitive) == 0) continue;
        
        hasher.addData(it.key().toUtf8());
        hasher.addData(it.value().toUtf8());
    }
    
    return hasher.result().toHex();
}

HistoryItem HistoryManager::findItemByHash(const QString& hash) {
    if (hash.isEmpty()) return HistoryItem();
    
    // 1. 缓存命中: 在内存中搜索 (如果启用持久化)
    // 优先返回最近的记录 (因为 m_memoryHistory 是按时间倒序排列的)
    for (const auto& item : m_memoryHistory) {
        if (item.contentHash == hash && item.result.success) 
            return item;
    }
    
    // 2. 缓存命中: 在 SQLite 数据库中搜索 (如果启用持久化)
    if (m_persistenceEnabled) {
        QSqlDatabase db = getDatabase();
        if (db.isOpen()) {
            QSqlQuery query(db);
            // 查找最近一次成功的记录
            query.prepare("SELECT * FROM history WHERE content_hash = :hash AND success = 1 ORDER BY timestamp DESC LIMIT 1");
            query.bindValue(":hash", hash);
            if (query.exec() && query.next()) {
                HistoryItem item;
                item.id = query.value("id").toLongLong(); // Add ID
                item.timestamp = QDateTime::fromMSecsSinceEpoch(query.value("timestamp").toLongLong());
                item.imagePath = query.value("image_path").toString();
                item.source = static_cast<SubmitSource>(query.value("source").toInt());
                item.result.success = true; // We queried for success=1
                item.result.fullText = query.value("full_text").toString();
                item.result.modelName = query.value("model_name").toString();
                item.result.processingTimeMs = query.value("processing_time_ms").toLongLong();
                item.result.errorMessage = query.value("error_message").toString();
                item.contentHash = hash;
                
                // 加载图片
                if (!item.imagePath.isEmpty() && QFile::exists(item.imagePath)) {
                    item.image.load(item.imagePath);
                }
                
                return item;
            }
        }
    }
    
    return HistoryItem(); // 未找到
}