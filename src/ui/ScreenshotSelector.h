#pragma once

#include <QWidget>
#include <QPoint>
#include <QRect>
#include <QPixmap>
#include <QImage>
#include <functional>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QPen>
#include <QColor>

// 区域选择窗口类（用于截图）
class ScreenshotSelector : public QWidget {
    Q_OBJECT

public:
    explicit ScreenshotSelector(std::function<void(const QImage&)> onAccepted, 
                                std::function<void()> onRejected,
                                QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QPoint m_startPoint;
    QRect m_selectionRect;
    QPixmap m_fullScreenshot;
    bool m_isSelecting;
    std::function<void(const QImage&)> m_onAccepted;
    std::function<void()> m_onRejected;
};