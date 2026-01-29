#pragma once

#include <QObject>
#include <QString>

class MainWindow;

class ThemeManager : public QObject {
    Q_OBJECT

public:
    explicit ThemeManager(QObject* parent = nullptr);
    
    // 应用主题到主窗口
    static void applyTheme(MainWindow* window, bool grayTheme);

private:
    // 具体的样式生成方法
    static QString getScrollBarStyle(bool grayTheme);
    static QString getListStyle(bool grayTheme, const QString& selectedBg, const QString& selectedText, const QString& hoverBg);
    static QString getTabStyle(bool grayTheme);
    static QString getComboBoxStyle(bool grayTheme);
    static QString getMainButtonStyle(bool grayTheme);
    static QString getRecognizeButtonStyle(bool grayTheme);
    static QString getResultButtonStyle(bool grayTheme);
    static QString getBatchNavStyle(bool grayTheme);
    static QString getBatchInfoStyle(bool grayTheme);
    static QString getCloseButtonStyle(bool grayTheme);
    static QString getDarkButtonStyle(const QString& btnDark, const QString& btnDarkHover);
    static QString getHintLabelStyle(bool grayTheme);
    static QString getEditStyle(bool grayTheme);
    static QString getResultTextStyle(bool grayTheme);
    static QString getConfigLabelStyle(bool grayTheme);
    static QString getSeparatorStyle(bool grayTheme);
    static QString getSidebarToggleStyle(const QString& color, const QString& hoverBg, const QString& hoverColor);
    static QString getSplitterStyle(const QString& color, const QString& border, const QString& hoverColor, const QString& hoverBorder);
    static QString getImageLabelStyle(bool grayTheme);
    static QString getHistoryListStyle(bool grayTheme);
    static QString getStatusBarStyle(bool grayTheme);
};