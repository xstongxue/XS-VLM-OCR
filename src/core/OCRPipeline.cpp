#include "OCRPipeline.h"
#include <QDebug>
#include <QMetaObject>

OCRPipeline::OCRPipeline(QObject *parent)
    : QObject(parent), m_currentAdapter(nullptr), 
    m_threadPool(QThreadPool::globalInstance())   // 默认用的是全局线程池的默认并发（≈CPU 核心数）
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

void OCRPipeline::submitImage(const QImage &image, SubmitSource source, const QString &prompt, const QString &contextId)
{
    if (!m_currentAdapter)
    {
        emit recognitionFailed("未选择模型适配器", image, source, contextId);
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

    emit recognitionStarted(image, source, contextId);

    // 创建异步任务
    OCRTask *task = new OCRTask(m_currentAdapter, image, source, prompt, contextId, this);

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
                 const QString &contextId,
                 QObject *receiver)
    : m_adapter(adapter), m_image(image), m_source(source), m_prompt(prompt), m_contextId(contextId), m_receiver(receiver)
{
    setAutoDelete(true); // 任务完成后自动删除
}

void OCRTask::run()
{
    if (!m_adapter)
    {
        // 直接发送信号（会自动使用队列连接）
        emit error("适配器为空", m_image, m_source, m_contextId);
        return;
    }

    qDebug() << "OCRTask: 在线程中运行" << QThread::currentThreadId();

    try
    {
        // 执行识别
        OCRResult result = m_adapter->recognize(m_image, m_prompt);
        result.contextId = m_contextId;

        // 发送结果信号
        if (result.success)
        {
            emit finished(result, m_image, m_source, m_contextId);
        }
        else
        {
            emit error(result.errorMessage, m_image, m_source, m_contextId);
        }
    }
    catch (const std::exception &e)
    {
        QString errorMsg = QString("异常: %1").arg(e.what());
        emit error(errorMsg, m_image, m_source, m_contextId);
    }
    catch (...)
    {
        emit error("未知异常", m_image, m_source, m_contextId);
    }
}