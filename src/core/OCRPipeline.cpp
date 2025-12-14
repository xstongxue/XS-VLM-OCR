#include "OCRPipeline.h"
#include <QDebug>
#include <QMetaObject>

OCRPipeline::OCRPipeline(QObject *parent)
    : QObject(parent), m_currentAdapter(nullptr), m_threadPool(QThreadPool::globalInstance())
{
    // 配置线程池
    // m_threadPool->setMaxThreadCount(4);
}

OCRPipeline::~OCRPipeline()
{
    // 等待所有任务完成
    m_threadPool->waitForDone();
}

void OCRPipeline::setCurrentAdapter(ModelAdapter *adapter)
{
    m_currentAdapter = adapter;
    if (m_currentAdapter)
    {
        qDebug() << "OCRPipeline: 当前适配器设置为" << m_currentAdapter->config().displayName;
    }
}

void OCRPipeline::submitImage(const QImage &image, SubmitSource source, const QString &prompt)
{
    if (!m_currentAdapter)
    {
        emit recognitionFailed("未选择模型适配器", image, source);
        return;
    }

    // 允许空图片（用于纯文本AI询问）
    if (image.isNull())
    {
        qDebug() << "OCRPipeline: 无图片，将进行纯文本AI询问";
    }
    else
    {
        qDebug() << "OCRPipeline: 提交图片"
                 << image.width() << "x" << image.height()
                 << "来源:" << static_cast<int>(source);
    }

    emit recognitionStarted(image, source);

    // 创建异步任务
    OCRTask *task = new OCRTask(m_currentAdapter, image, source, prompt, this);

    // 连接信号（使用 Qt::QueuedConnection 确保跨线程安全）
    connect(task, &OCRTask::finished, this, &OCRPipeline::recognitionCompleted, Qt::QueuedConnection);
    connect(task, &OCRTask::error, this, &OCRPipeline::recognitionFailed, Qt::QueuedConnection);

    // 提交到线程池
    m_threadPool->start(task);
}

// OCRTask 实现
OCRTask::OCRTask(ModelAdapter *adapter,
                 const QImage &image,
                 SubmitSource source,
                 const QString &prompt,
                 QObject *receiver)
    : m_adapter(adapter), m_image(image), m_source(source), m_prompt(prompt), m_receiver(receiver)
{
    setAutoDelete(true); // 任务完成后自动删除
}

void OCRTask::run()
{
    if (!m_adapter)
    {
        // 直接发送信号（会自动使用队列连接）
        emit error("适配器为空", m_image, m_source);
        return;
    }

    qDebug() << "OCRTask: 在线程中运行" << QThread::currentThreadId();

    try
    {
        // 执行识别
        OCRResult result = m_adapter->recognize(m_image, m_prompt);

        // 发送结果信号
        if (result.success)
        {
            emit finished(result, m_image, m_source);
        }
        else
        {
            emit error(result.errorMessage, m_image, m_source);
        }
    }
    catch (const std::exception &e)
    {
        QString errorMsg = QString("异常: %1").arg(e.what());
        emit error(errorMsg, m_image, m_source);
    }
    catch (...)
    {
        emit error("未知异常", m_image, m_source);
    }
}