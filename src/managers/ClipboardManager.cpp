#include "ClipboardManager.h"
#include <QGuiApplication>
#include <QMimeData>
#include <QDebug>

ClipboardManager::ClipboardManager(QObject* parent)
    : QObject(parent)
    , m_clipboard(QGuiApplication::clipboard())
    , m_monitoring(false)
{
    // QClipboard::dataChanged 信号不带参数，需要使用 lambda 或者重载
    connect(m_clipboard, &QClipboard::dataChanged, this, [this]() {
        onClipboardChanged(QClipboard::Clipboard);
    });
}

void ClipboardManager::copyText(const QString& text)
{
    if (text.isEmpty()) {
        qWarning() << "ClipboardManager: 无法复制空文本";
        return;
    }
    
    m_clipboard->setText(text);
    m_lastText = text;
    qDebug() << "ClipboardManager: 文本已复制到剪贴板,长度:" << text.length();
}

void ClipboardManager::copyImage(const QImage& image)
{
    if (image.isNull()) {
        qWarning() << "ClipboardManager: 无法复制空图像";
        return;
    }
    
    m_clipboard->setImage(image);
    m_lastImage = image;
    qDebug() << "ClipboardManager: 图像已复制到剪贴板,大小:" << image.size();
}

QString ClipboardManager::getText() const
{
    return m_clipboard->text();
}

QImage ClipboardManager::getImage() const
{
    return m_clipboard->image();
}

bool ClipboardManager::hasImage() const
{
    const QMimeData* mimeData = m_clipboard->mimeData();
    return mimeData && mimeData->hasImage();
}

bool ClipboardManager::hasText() const
{
    const QMimeData* mimeData = m_clipboard->mimeData();
    return mimeData && mimeData->hasText();
}

void ClipboardManager::setMonitoringEnabled(bool enabled)
{
    m_monitoring = enabled;
    qDebug() << "ClipboardManager: 监听" << (enabled ? "已启用" : "已禁用");
}

void ClipboardManager::onClipboardChanged(QClipboard::Mode mode)
{
    if (!m_monitoring || mode != QClipboard::Clipboard) {
        return;
    }
    
    emit clipboardChanged();
    
    // 检查是否有新图像
    if (hasImage()) {
        QImage image = getImage();
        if (!image.isNull() && image != m_lastImage) {
            m_lastImage = image;
            qDebug() << "ClipboardManager: 新图像已检测到,大小:" << image.size();
            emit imageAvailable(image);
        }
    }
    
    // 检查是否有新文本
    if (hasText()) {
        QString text = getText();
        if (!text.isEmpty() && text != m_lastText) {
            m_lastText = text;
            qDebug() << "ClipboardManager: 新文本已检测到,长度:" << text.length();
            emit textAvailable(text);
        }
    }
}