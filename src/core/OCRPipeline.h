#pragma once
#include <QObject>
#include <QImage>
#include <QPointer>
#include <QThreadPool>
#include "ModelAdapter.h"
#include "OCRResult.h"

// OCR 处理流水线
// 负责调度模型、异步执行、结果回调
class OCRPipeline : public QObject {
    Q_OBJECT
    
public:
    explicit OCRPipeline(QObject* parent = nullptr);
    ~OCRPipeline() override;
    
    // 设置当前使用的模型
    void setCurrentAdapter(ModelAdapter* adapter);
    
    // 获取当前模型
    ModelAdapter* currentAdapter() const { return m_currentAdapter; }
    
    // 提交图像进行识别（异步）
    void submitImage(const QImage& image, 
                    SubmitSource source = SubmitSource::Upload,
                    const QString& prompt = QString(),
                    const QString& contextId = QString());
    
signals:
    // 识别开始
    void recognitionStarted(const QImage& image, SubmitSource source, const QString& contextId);
    
    // 识别完成
    void recognitionCompleted(const OCRResult& result, const QImage& image, SubmitSource source, const QString& contextId);
    
    // 识别失败
    void recognitionFailed(const QString& error, const QImage& image, SubmitSource source, const QString& contextId);
    
    // 进度更新（可选）
    void progressUpdated(int percentage);
    
private:
    ModelAdapter* m_currentAdapter;
    QThreadPool* m_threadPool;
    QString m_currentPrompt;
};
// OCR 异步任务
class OCRTask : public QObject, public QRunnable {
    Q_OBJECT
    
public:
    OCRTask(ModelAdapter* adapter, 
           const QImage& image,
           SubmitSource source,
           const QString& prompt,
           const QString& contextId,
           QObject* receiver);
    
    void run() override;
    
signals:
    void finished(const OCRResult& result, const QImage& image, SubmitSource source, const QString& contextId);
    void error(const QString& errorMsg, const QImage& image, SubmitSource source, const QString& contextId);
    
private:
    QPointer<ModelAdapter> m_adapter;
    QImage m_image;
    SubmitSource m_source;
    QString m_prompt;
    QString m_contextId;
    QObject* m_receiver;
};