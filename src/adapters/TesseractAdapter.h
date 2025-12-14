#pragma once
#include "../core/ModelAdapter.h"
#include <QMutex>

// Tesseract OCR 适配器
// 注意：这是一个简化版本，使用命令行调用
// 生产环境建议使用 libtesseract C++ API
class TesseractAdapter : public ModelAdapter {
    Q_OBJECT
    
public:
    explicit TesseractAdapter(const ModelConfig& config, QObject* parent = nullptr);
    ~TesseractAdapter() override;
    
    bool initialize() override;
    OCRResult recognize(const QImage& image, const QString& prompt = QString()) override;
    bool isInitialized() const override { return m_initialized; }
    
private:
    // 预处理图像（增强OCR效果）
    QImage preprocessImage(const QImage& image);
    
    // 通过命令行调用 tesseract
    QString runTesseractCommand(const QString& imagePath, const QString& lang);
    
    // 使用 libtesseract API（如果链接了库）
    QString runTesseractAPI(const QImage& image, const QString& lang);
    
    bool m_initialized;
    QString m_tesseractPath;  // tesseract 可执行文件路径
    QString m_language;       // 语言代码（如 chi_sim, eng）
    QMutex m_mutex;           // 线程安全
};
