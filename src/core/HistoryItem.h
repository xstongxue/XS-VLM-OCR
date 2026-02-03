#pragma once
#include <QImage>
#include <QDateTime>
#include "OCRResult.h"

// 识别历史记录项
struct HistoryItem {
    long long id = -1;   // 数据库 ID
    QImage image;
    OCRResult result;
    SubmitSource source; // 提交源: 上传、粘贴、截图
    QDateTime timestamp;
    QString imagePath;   // 本地保存的图片路径，用于持久化
    QString contentHash; // 内容哈希 (image + prompt + model name + model params)，用于缓存去重
    bool persisted = false; // 是否已持久化到数据库
};