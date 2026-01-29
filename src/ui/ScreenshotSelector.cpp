#include "ScreenshotSelector.h"
#include <QGuiApplication>
#include <QScreen>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>

ScreenshotSelector::ScreenshotSelector(std::function<void(const QImage&)> onAccepted, 
                                       std::function<void()> onRejected,
                                       QWidget* parent) 
    : QWidget(parent), m_startPoint(), m_selectionRect(), m_isSelecting(false),
      m_onAccepted(onAccepted), m_onRejected(onRejected) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setCursor(Qt::CrossCursor);
    
    // 获取所有屏幕的几何信息
    QRect totalRect;
    for (QScreen* screen : QGuiApplication::screens()) {
        totalRect = totalRect.united(screen->geometry());
    }
    setGeometry(totalRect);
    
    // 截取整个屏幕作为背景
    // 组装所有屏幕的截图为一张大图（支持多屏）
    const QList<QScreen*> screens = QGuiApplication::screens();
    if (!screens.isEmpty()) {
        QPixmap composed(totalRect.size());
        composed.fill(Qt::transparent);
        QPainter p(&composed);
        for (QScreen* s : screens) {
            // 抓取该屏幕画面
            QPixmap pm = s->grabWindow(0);
            // 统一为逻辑像素，避免不同缩放比导致错位
            pm.setDevicePixelRatio(1.0);
            pm = pm.scaled(s->geometry().size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            // 放到虚拟桌面的对应位置
            const QPoint offset = s->geometry().topLeft() - totalRect.topLeft();
            p.drawPixmap(offset, pm);
        }
        p.end();
        m_fullScreenshot = composed;
    }
    
    show();
    raise();
    activateWindow();
}

void ScreenshotSelector::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    
    // 绘制半透明遮罩
    painter.fillRect(rect(), QColor(0, 0, 0, 100));
    
    // 绘制截图背景
    if (!m_fullScreenshot.isNull()) {
        painter.drawPixmap(0, 0, m_fullScreenshot);
        // 再次绘制遮罩
        painter.fillRect(rect(), QColor(0, 0, 0, 100));
    }
    
    // 绘制选中区域（高亮显示）
    if (!m_selectionRect.isEmpty()) {
        // 清除选中区域的遮罩
        QRegion mask = QRegion(rect()) - QRegion(m_selectionRect);
        painter.setClipRegion(mask);
        painter.fillRect(rect(), QColor(0, 0, 0, 150));
        painter.setClipping(false);
        
        // 绘制选中区域边框
        painter.setPen(QPen(QColor(0, 120, 215), 2));
        painter.drawRect(m_selectionRect);
        
        // 显示尺寸信息
        QString sizeText = QString("%1 x %2")
            .arg(m_selectionRect.width())
            .arg(m_selectionRect.height());
        QFont font = painter.font();
        font.setBold(true);
        font.setPixelSize(14);
        painter.setFont(font);
        QRect textRect = m_selectionRect;
        textRect.setTop(textRect.top() - 25);
        textRect.setHeight(20);
        painter.fillRect(textRect.adjusted(-2, -2, 2, 2), QColor(0, 120, 215));
        painter.setPen(Qt::white);
        painter.drawText(textRect, Qt::AlignCenter, sizeText);
    }
}

void ScreenshotSelector::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_startPoint = event->pos();
        m_selectionRect = QRect();
        m_isSelecting = true;
        update();
    }
}

void ScreenshotSelector::mouseMoveEvent(QMouseEvent* event) {
    if (m_isSelecting) {
        QPoint endPoint = event->pos();
        m_selectionRect = QRect(m_startPoint, endPoint).normalized();
        update();
    }
}

void ScreenshotSelector::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_isSelecting) {
        m_isSelecting = false;
        if (!m_selectionRect.isEmpty() && m_selectionRect.width() > 10 && m_selectionRect.height() > 10) {
            QImage selectedImage = m_fullScreenshot.copy(m_selectionRect).toImage();
            close();
            if (m_onAccepted) {
                m_onAccepted(selectedImage);
            }
        } else {
            // 选择区域太小，取消
            close();
            if (m_onRejected) {
                m_onRejected();
            }
        }
    }
}

void ScreenshotSelector::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        close();
        if (m_onRejected) {
            m_onRejected();
        }
    } else {
        QWidget::keyPressEvent(event);
    }
}