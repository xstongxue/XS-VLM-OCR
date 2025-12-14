#pragma once
#include <QString>
#include <QVector>
#include <QRectF>
#include <QDateTime>
#include <QMetaType>

// OCR识别结果中的单个文本块
struct TextBlock {
    QString text;           // 识别出的文本
    QRectF boundingBox;     // 边界框（归一化坐标 0-1）
    float confidence;       // 置信度 0-1-
    
    TextBlock() : confidence(0.0f) {}
};

// 完整的OCR识别结果
struct OCRResult {
    bool success;                    // 识别是否成功
    QString errorMessage;            // 错误信息（如果失败）
    QVector<TextBlock> textBlocks;   // 识别的文本块列表
    QString fullText;                // 合并后的完整文本
    QString modelName;               // 使用的模型名称
    QDateTime timestamp;             // 识别时间戳
    qint64 processingTimeMs;         // 处理耗时（毫秒）
    
    OCRResult() : success(false), processingTimeMs(0) {
        timestamp = QDateTime::currentDateTime();
    }
    
    // 从文本块合并完整文本
    void mergeFullText() {
        QStringList lines;
        for (const auto& block : textBlocks) {
            if (!block.text.trimmed().isEmpty()) {
                lines.append(block.text);
            }
        }
        fullText = lines.join("\n");
    }
};

// 图片提交来源
enum class SubmitSource {
    Upload,      // 手动上传
    Paste,       // 剪贴板粘贴
    Shortcut,    // 快捷键截图
    DragDrop     // 拖拽
};

// 注册元类型，以便在信号槽中使用
Q_DECLARE_METATYPE(OCRResult)
Q_DECLARE_METATYPE(QImage)
Q_DECLARE_METATYPE(SubmitSource)