#include "HistoryManager.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

HistoryManager::HistoryManager(QObject* parent) : QObject(parent) {
    m_historyDir = QDir::currentPath() + "/history";
    m_imagesDir = m_historyDir + "/images";
    m_historyFile = m_historyDir + "/history.json";
    ensureDirectories();
}

HistoryManager::~HistoryManager() {
    saveHistory();
}

void HistoryManager::ensureDirectories() {
    QDir dir(m_historyDir);
    if (!dir.exists()) dir.mkpath(".");
    QDir imgDir(m_imagesDir);
    if (!imgDir.exists()) imgDir.mkpath(".");
}

void HistoryManager::addHistoryItem(const HistoryItem& item) {
    HistoryItem newItem = item;
    
    // 只有在启用持久化时才保存图片到磁盘
    if (m_persistenceEnabled && newItem.imagePath.isEmpty()) {
        QString fileName = QString("%1.png").arg(newItem.timestamp.toString("yyyyMMdd_HHmmss_zzz"));
        QString filePath = m_imagesDir + "/" + fileName;
        
        if (newItem.image.save(filePath)) {
            newItem.imagePath = filePath;
        } else {
            qWarning() << "HistoryManager: Failed to save image:" << filePath;
        }
    }
    
    m_history.prepend(newItem); // 添加到开头
    
    // 限制历史记录数量
    if (m_history.size() > m_maxHistory) {
        // 删除旧的图片文件
        for (int i = m_maxHistory; i < m_history.size(); ++i) {
             if (m_persistenceEnabled && !m_history[i].imagePath.isEmpty()) {
                 QFile::remove(m_history[i].imagePath);
             }
        }
        m_history.resize(m_maxHistory);
    }
    
    if (m_persistenceEnabled) {
        saveHistory();
    }
    emit historyChanged();
}

void HistoryManager::setMaxHistory(int max) {
    if (max <= 0) return;
    m_maxHistory = max;

    // 如果当前数量超过新的限制，则裁剪
    if (m_history.size() > m_maxHistory) {
        // 删除旧的图片文件
        for (int i = m_maxHistory; i < m_history.size(); ++i) {
             if (m_persistenceEnabled && !m_history[i].imagePath.isEmpty()) {
                 QFile::remove(m_history[i].imagePath);
             }
        }
        m_history.resize(m_maxHistory);
        
        if (m_persistenceEnabled) {
            saveHistory();
        }
        emit historyChanged();
    }
}

int HistoryManager::getMaxHistory() const {
    return m_maxHistory;
}

void HistoryManager::setPersistenceEnabled(bool enabled) {
    if (m_persistenceEnabled == enabled) return;
    m_persistenceEnabled = enabled;
    
    if (enabled) {
        // 如果启用，补救保存之前未保存的图片
        bool changed = false;
        for (auto& item : m_history) {
             if (item.imagePath.isEmpty() && !item.image.isNull()) {
                 QString fileName = QString("%1.png").arg(item.timestamp.toString("yyyyMMdd_HHmmss_zzz"));
                 QString filePath = m_imagesDir + "/" + fileName;
                 if (item.image.save(filePath)) {
                     item.imagePath = filePath;
                     changed = true;
                 }
             }
        }
        saveHistory();
        if (changed) {
            emit historyChanged();
        }
    }
}

bool HistoryManager::isPersistenceEnabled() const {
    return m_persistenceEnabled;
}

void HistoryManager::clearHistory() {
    // 1. 如果启用了持久化，从内存列表中删除关联的图片
    if (m_persistenceEnabled) {
        for (const auto& item : m_history) {
            if (!item.imagePath.isEmpty()) {
                QFile::remove(item.imagePath);
            }
        }
    } 
    // 2. 如果未启用持久化，但存在历史文件，说明有残留的历史记录需要清理
    // 用户期望"清空历史"能彻底清理磁盘上的内容
    else if (QFile::exists(m_historyFile)) {
        QFile file(m_historyFile);
        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            file.close();
            if (doc.isArray()) {
                QJsonArray arr = doc.array();
                for (const QJsonValue& val : arr) {
                    QString imgPath = val.toObject()["imagePath"].toString();
                    if (!imgPath.isEmpty() && QFile::exists(imgPath)) {
                        QFile::remove(imgPath);
                    }
                }
            }
        }
    }

    m_history.clear();
    
    // 3. 总是删除 history.json 文件，确保彻底清空
    if (QFile::exists(m_historyFile)) {
        QFile::remove(m_historyFile);
    }
    
    emit historyChanged();
}

void HistoryManager::removeHistoryItem(int index) {
    if (index >= 0 && index < m_history.size()) {
        if (m_persistenceEnabled && !m_history[index].imagePath.isEmpty()) {
            QFile::remove(m_history[index].imagePath);
        }
        m_history.remove(index);
        saveHistory();
        emit historyChanged();
    }
}

const QVector<HistoryItem>& HistoryManager::getHistory() const {
    return m_history;
}

void HistoryManager::saveHistory() {
    if (!m_persistenceEnabled) return;

    QJsonArray historyArray;
    
    for (const auto& item : m_history) {
        QJsonObject obj;
        obj["timestamp"] = item.timestamp.toString(Qt::ISODate);
        obj["imagePath"] = item.imagePath;
        obj["source"] = static_cast<int>(item.source);
        
        QJsonObject resultObj;
        resultObj["success"] = item.result.success;
        resultObj["fullText"] = item.result.fullText;
        resultObj["modelName"] = item.result.modelName;
        resultObj["processingTimeMs"] = item.result.processingTimeMs;
        resultObj["errorMessage"] = item.result.errorMessage;
        
        // 简单序列化 textBlocks，如果有必要可以详细序列化
        // 这里只保存 fullText 就足够一般显示了
        
        obj["result"] = resultObj;
        historyArray.append(obj);
    }
    
    QJsonDocument doc(historyArray);
    QFile file(m_historyFile);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
    } else {
        qWarning() << "HistoryManager: Failed to save history file:" << m_historyFile;
    }
}

void HistoryManager::loadHistory() {
    QFile file(m_historyFile);
    if (!file.exists()) return;
    
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "HistoryManager: Failed to open history file:" << m_historyFile;
        return;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) return;
    
    m_history.clear();
    QJsonArray historyArray = doc.array();
    
    for (const QJsonValue& val : historyArray) {
        QJsonObject obj = val.toObject();
        HistoryItem item;
        item.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODate);
        item.imagePath = obj["imagePath"].toString();
        item.source = static_cast<SubmitSource>(obj["source"].toInt());
        
        // 加载图片
        if (!item.imagePath.isEmpty() && QFile::exists(item.imagePath)) {
            item.image.load(item.imagePath);
        }
        
        QJsonObject resultObj = obj["result"].toObject();
        item.result.success = resultObj["success"].toBool();
        item.result.fullText = resultObj["fullText"].toString();
        item.result.modelName = resultObj["modelName"].toString();
        item.result.processingTimeMs = resultObj["processingTimeMs"].toVariant().toLongLong();
        item.result.errorMessage = resultObj["errorMessage"].toString();
        
        m_history.append(item);
    }
    
    emit historyChanged();
}