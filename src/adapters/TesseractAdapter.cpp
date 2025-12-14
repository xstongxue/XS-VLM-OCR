#include "TesseractAdapter.h"
#include <QProcess>
#include <QTemporaryFile>
#include <QDebug>
#include <QElapsedTimer>
#include <QBuffer>
#include <QStandardPaths>
#include <QDir>

TesseractAdapter::TesseractAdapter(const ModelConfig &config, QObject *parent)
    : ModelAdapter(config, parent), m_initialized(false)
{
    // 从配置中读取语言参数
    m_language = m_config.params.value("lang", "chi_sim+eng");
}

TesseractAdapter::~TesseractAdapter()
{
}

bool TesseractAdapter::initialize()
{
    QMutexLocker locker(&m_mutex);

    qDebug() << "=== Initializing TesseractAdapter ===";

    // 查找 tesseract 可执行文件
    // Windows: tesseract.exe, Linux/Mac: tesseract
#ifdef Q_OS_WIN
    QString exeName = "tesseract.exe";
#else
    QString exeName = "tesseract";
#endif

    // 1. 尝试从配置中指定的路径
    if (m_config.params.contains("path"))
    {
        m_tesseractPath = m_config.params["path"];
        qDebug() << "Using configured path:" << m_tesseractPath;
    }
    else
    {
        // 2. 尝试从系统 PATH 查找
        m_tesseractPath = QStandardPaths::findExecutable(exeName);
        if (!m_tesseractPath.isEmpty())
        {
            qDebug() << "Found in PATH:" << m_tesseractPath;
        }
    }

    if (m_tesseractPath.isEmpty())
    {
        qWarning() << "========================================";
        qWarning() << "ERROR: Tesseract not found!";
        qWarning() << "========================================";
        qWarning() << "请安装 Tesseract OCR:";
        qWarning() << "Windows: choco install tesseract";
        qWarning() << "  或下载: https://github.com/UB-Mannheim/tesseract/wiki";
        qWarning() << "Linux: sudo apt-get install tesseract-ocr tesseract-ocr-chi-sim";
        qWarning() << "";
        qWarning() << "安装后确保 tesseract 在系统 PATH 中";
        qWarning() << "========================================";
        return false;
    }

    // 测试 tesseract 是否可用
    qDebug() << "Testing tesseract executable:" << m_tesseractPath;
    QProcess testProcess;
    testProcess.start(m_tesseractPath, QStringList() << "--version");

    if (!testProcess.waitForFinished(5000))
    {
        qWarning() << "Tesseract test timeout!";
        qWarning() << "Process state:" << testProcess.state();
        qWarning() << "Error:" << testProcess.errorString();
        return false;
    }

    if (testProcess.exitCode() != 0)
    {
        qWarning() << "Tesseract returned error code:" << testProcess.exitCode();
        qWarning() << "Stderr:" << testProcess.readAllStandardError();
        return false;
    }

    QString output = testProcess.readAllStandardOutput();
    qDebug() << "Tesseract version:" << output.split('\n').first();
    qDebug() << "Language:" << m_language;

    m_initialized = true;
    qDebug() << "TesseractAdapter initialized successfully!";
    return true;
}

QImage TesseractAdapter::preprocessImage(const QImage &image)
{
    // 基本预处理：转换为灰度图
    QImage processed = image.convertToFormat(QImage::Format_Grayscale8);

    // 可选：放大小图片以提高识别率
    if (processed.width() < 800)
    {
        int scale = 800 / processed.width() + 1;
        processed = processed.scaled(processed.width() * scale,
                                     processed.height() * scale,
                                     Qt::KeepAspectRatio,
                                     Qt::SmoothTransformation);
    }

    return processed;
}

QString TesseractAdapter::runTesseractCommand(const QString &imagePath, const QString &lang)
{
    QProcess process;

    // tesseract input.png stdout -l chi_sim+eng
    QStringList args;
    args << imagePath << "stdout" << "-l" << lang;

    qDebug() << "Running tesseract:" << m_tesseractPath << args.join(" ");

    process.start(m_tesseractPath, args);

    if (!process.waitForFinished(30000))
    { // 30秒超时
        qWarning() << "Tesseract process timeout";
        process.kill();
        return QString();
    }

    if (process.exitCode() != 0)
    {
        QString errorOutput = QString::fromUtf8(process.readAllStandardError());
        qWarning() << "Tesseract error (exit code" << process.exitCode() << "):" << errorOutput;
        return QString();
    }

    QString result = QString::fromUtf8(process.readAllStandardOutput());
    qDebug() << "Tesseract output length:" << result.length();

    return result;
}

QString TesseractAdapter::runTesseractAPI(const QImage &image, const QString &lang)
{
    // TODO: 如果链接了 libtesseract，在这里使用 API
    // 这里暂时返回空，表示未实现
    Q_UNUSED(image);
    Q_UNUSED(lang);
    return QString();
}

OCRResult TesseractAdapter::recognize(const QImage &image, const QString &prompt)
{
    Q_UNUSED(prompt); // Tesseract 不支持 prompt

    QMutexLocker locker(&m_mutex);
    QElapsedTimer timer;
    timer.start();

    OCRResult result;
    result.modelName = m_config.displayName;

    if (!m_initialized)
    {
        result.success = false;
        result.errorMessage = "Tesseract not initialized";
        return result;
    }

    if (image.isNull())
    {
        result.success = false;
        result.errorMessage = "Invalid image";
        return result;
    }

    try
    {
        // 预处理图像
        QImage processed = preprocessImage(image);

        // 保存到临时文件
        QTemporaryFile tempFile;
        tempFile.setAutoRemove(true);
        tempFile.setFileTemplate(QDir::temp().filePath("vlm_ocr_XXXXXX.png"));

        if (!tempFile.open())
        {
            result.success = false;
            result.errorMessage = "Failed to create temp file";
            qWarning() << "Failed to create temp file";
            return result;
        }

        QString tempPath = tempFile.fileName();
        tempFile.close(); // 关闭文件以便 tesseract 可以读取

        if (!processed.save(tempPath, "PNG"))
        {
            result.success = false;
            result.errorMessage = "Failed to save temp image";
            qWarning() << "Failed to save image to:" << tempPath;
            return result;
        }

        qDebug() << "Temp image saved to:" << tempPath;

        // 调用 tesseract
        QString text = runTesseractCommand(tempPath, m_language);

        // 即使结果为空也不应该报错（可能图片没有文字）
        if (text.isEmpty())
        {
            qDebug() << "Tesseract returned empty result (no text detected)";
        }

        // 构建结果
        result.success = true;
        result.fullText = text.trimmed();

        // 如果有文本，创建文本块
        if (!result.fullText.isEmpty())
        {
            TextBlock block;
            block.text = result.fullText;
            block.boundingBox = QRectF(0, 0, 1, 1); // 整个图像
            block.confidence = 0.8f;                // Tesseract 默认不返回置信度
            result.textBlocks.append(block);
        }

        result.processingTimeMs = timer.elapsed();

        qDebug() << "Tesseract OCR completed in" << result.processingTimeMs << "ms";
        qDebug() << "Recognized text length:" << result.fullText.length();
    }
    catch (...)
    {
        result.success = false;
        result.errorMessage = "Exception occurred during OCR";
        qWarning() << "Exception in TesseractAdapter::recognize";
    }

    return result;
}