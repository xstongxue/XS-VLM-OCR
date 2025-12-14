#pragma once
#include <QObject>
#include <QClipboard>
#include <QImage>

// 剪贴板管理器
// 负责读取/写入系统剪贴板
class ClipboardManager : public QObject {
    Q_OBJECT
    
public:
    explicit ClipboardManager(QObject* parent = nullptr);
    ~ClipboardManager() override = default;
    
    // 复制文本到剪贴板
    void copyText(const QString& text);
    
    // 复制图像到剪贴板
    void copyImage(const QImage& image);
    
    // 从剪贴板获取文本
    QString getText() const;
    
    // 从剪贴板获取图像
    QImage getImage() const;
    
    // 检查剪贴板是否包含图像
    bool hasImage() const;
    
    // 检查剪贴板是否包含文本
    bool hasText() const;
    
    // 启用/禁用监听剪贴板变化
    void setMonitoringEnabled(bool enabled);
    
signals:
    // 剪贴板内容变化
    void clipboardChanged();
    
    // 剪贴板有新图像
    void imageAvailable(const QImage& image);
    
    // 剪贴板有新文本
    void textAvailable(const QString& text);
    
private slots:
    void onClipboardChanged(QClipboard::Mode mode);
    
private:
    QClipboard* m_clipboard;
    bool m_monitoring;
    QImage m_lastImage;
    QString m_lastText;
};