#include "ThemeManager.h"
#include "../ui/MainWindow.h"
#include "../ui/SidebarWidget.h"
#include <QScrollBar>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QTabWidget>
#include <QTextEdit>
#include <QListWidget>
#include <QFrame>
#include <QSplitter>
#include <QStatusBar>
#include <QScrollArea>
#include <QTabBar>
#include <QToolButton>
#include <QAbstractItemView>

ThemeManager::ThemeManager(QObject* parent) : QObject(parent) {}

void ThemeManager::applyTheme(MainWindow* window, bool grayTheme)
{
    if (!window) return;

    window->m_isGrayTheme = grayTheme;
    
    QString bg, sidebarBg, sidebarText, navHover, navChecked, navCheckedBorder;
    QString cardBg, cardBorder, cardTitle, textColor, btnDark, btnDarkHover;
    
    if (grayTheme) {
        // 灰主题（深色系）
        bg = "#1b1b1b";
        sidebarBg = "#2c2c2c";
        sidebarText = "#e0e0e0";
        navHover = "#333333";
        navChecked = "#383838";
        navCheckedBorder = "#ffffff";
        cardBg = "#242424";
        cardBorder = "#3a3a3a";
        cardTitle = "#e6e6e6";
        textColor = "#e0e0e0";
        btnDark = "#444444";
        btnDarkHover = "#555555";
    } else {
        // 白主题（最简洁的黑白配色）
        bg = "#ffffff";           // 主内容区：纯白
        sidebarBg = "#ffffff";    // 侧边栏：纯白
        sidebarText = "#000000";  // 侧边栏文字：纯黑
        navHover = "#f5f5f5";     // 导航悬停：浅灰
        navChecked = "#e0e0e0";   // 导航选中：浅灰背景
        navCheckedBorder = "#000000"; // 选中边框：黑色
        cardBg = "#ffffff";       // 卡片：纯白
        cardBorder = "#e0e0e0";   // 卡片边框：浅灰
        cardTitle = "#000000";    // 卡片标题：纯黑
        textColor = "#000000";    // 正文：纯黑
        btnDark = "#000000";      // 按钮：纯黑
        btnDarkHover = "#333333"; // 按钮悬停：深灰
    }
    
    if (window->m_themeToggleBtn)
    {
        // 按钮使用图标，不再显示文本
        // 单独设置主题切换按钮样式（不受侧边栏通用样式影响）
        if (grayTheme) {
            // 灰主题：深色按钮
            window->m_themeToggleBtn->setStyleSheet(
                "QPushButton { "
                "  padding: 10px 15px; "
                "  font-size: 10pt; "
                "  color: #e0e0e0; "
                "  background: rgba(0, 0, 0, 0.3); "
                "  border: 1px solid rgba(255, 255, 255, 0.2); "
                "  border-radius: 6px; "
                "  margin: 10px;"
                "}"
                "QPushButton:hover { "
                "  background: rgba(0, 0, 0, 0.4); "
                "  color: #ffffff;"
                "}"
            );
        } else {
            // 白主题：简洁的灰色按钮
            window->m_themeToggleBtn->setStyleSheet(
                "QPushButton { "
                "  padding: 10px 15px; "
                "  font-size: 10pt; "
                "  color: #000000; "
                "  background: #f5f5f5; "
                "  border: 1px solid #e0e0e0; "
                "  border-radius: 6px; "
                "  margin: 10px;"
                "}"
                "QPushButton:hover { "
                "  background: #e0e0e0; "
                "  color: #000000;"
                "}"
            );
        }
    }
    
    if (window->m_contentPanel)
    {
        window->m_contentPanel->setStyleSheet(QString("QWidget { background-color: %1; }").arg(bg));
    }
    
    if (window->m_centralWidget)
    {
        window->m_centralWidget->setStyleSheet(QString("QWidget { background-color: %1; }").arg(bg));
    }
    
    if (window->m_sidebarPanel)
    {
        // 设置侧边栏背景
        window->m_sidebarPanel->setStyleSheet(QString("QWidget { background: %1; }").arg(sidebarBg));
        window->m_sidebarPanel->updateTheme(grayTheme);
    }
    
    // 卡片统一样式（在 createCard 中打了 themeCard 标记）
    auto cards = window->findChildren<QWidget*>();
    for (QWidget* w : cards)
    {
        if (w->property("themeCard").toBool())
        {
            w->setStyleSheet(
                QString(
                    "QWidget { "
                    "  background: %1; "
                    "  border: 1px solid %2; "
                    "  border-radius: 12px; "
                    "  padding: 20px;"
                    "  color: %3;"
                    "}"
                    "QLabel { color: %4; }"
                )
                    .arg(cardBg)
                    .arg(cardBorder)
                    .arg(textColor)
                    .arg(cardTitle));
        }
    }
    
    // 主页面按钮（上传/粘贴/识别/复制等）根据主题切换样式
    if (window->m_uploadBtn && window->m_pasteBtn)
    {
        QString mainBtnStyle = getMainButtonStyle(grayTheme);
        window->m_uploadBtn->setStyleSheet(mainBtnStyle);
        window->m_pasteBtn->setStyleSheet(mainBtnStyle);
    }
    
    if (window->m_recognizeBtn)
    {
        window->m_recognizeBtn->setStyleSheet(getRecognizeButtonStyle(grayTheme));
    }
    
    if (window->m_copyResultBtn && window->m_exportBtn)
    {
        QString resultBtnStyle = getResultButtonStyle(grayTheme);
        window->m_copyResultBtn->setStyleSheet(resultBtnStyle);
        window->m_exportBtn->setStyleSheet(resultBtnStyle);
        if (window->m_previewResultBtn)
            window->m_previewResultBtn->setStyleSheet(resultBtnStyle);
    }
    
    // 批量左右切换按钮 & 信息标签 & 关闭按钮
    {
        QString batchNavStyle = getBatchNavStyle(grayTheme);
        if (window->m_prevImageBtn) window->m_prevImageBtn->setStyleSheet(batchNavStyle);
        if (window->m_nextImageBtn) window->m_nextImageBtn->setStyleSheet(batchNavStyle);

        if (window->m_batchInfoLabel) {
            window->m_batchInfoLabel->setStyleSheet(getBatchInfoStyle(grayTheme));
        }

        if (window->m_closeImageBtn) {
            window->m_closeImageBtn->setStyleSheet(getCloseButtonStyle(grayTheme));
        }
    }
    
    // 其他按钮（深色风格）
    auto buttons = window->findChildren<QPushButton*>();
    for (QPushButton* btn : buttons)
    {
        if (btn == window->m_navHomeBtn || btn == window->m_navHistoryBtn || btn == window->m_navSettingsBtn)
            continue; // 已在侧边栏样式中处理
        if (btn == window->m_themeToggleBtn)
            continue; // 切换按钮自身文字已处理
        if (btn == window->m_uploadBtn || btn == window->m_pasteBtn || btn == window->m_recognizeBtn || 
            btn == window->m_copyResultBtn || btn == window->m_exportBtn || btn == window->m_previewResultBtn)
            continue; // 主页面按钮已单独处理
        if (btn == window->m_prevImageBtn || btn == window->m_nextImageBtn)
            continue; // 批量左右切换按钮使用自定义浅色样式
        if (btn == window->m_closeImageBtn)
            continue; // 关闭按钮已自定义
        
        btn->setStyleSheet(getDarkButtonStyle(btnDark, btnDarkHover));
    }
    
    // 标签统一文字颜色（排除配置区域的标签，它们有专门样式）
    auto labels = window->findChildren<QLabel*>();
    for (QLabel* lbl : labels)
    {
        // 跳过配置区域的标签（"模型"、"提示词"），它们有专门样式
        QWidget* parent = lbl->parentWidget();
        if (parent && parent->property("configWidget").toBool())
        {
            continue;
        }
        // 结果提示标签使用完全透明的样式
        if (lbl->property("resultHint").toBool())
        {
            lbl->setStyleSheet(getHintLabelStyle(grayTheme));
            continue;
        }
        lbl->setStyleSheet(QString("QLabel { color: %1; background: transparent; }").arg(textColor));
    }
    
    // 更新侧边栏配置组件样式
    if (window->m_modelComboBox)
    {
        window->m_modelComboBox->setStyleSheet(getComboBoxStyle(grayTheme));
    }
    
    // 更新提示词选项卡样式
    if (window->m_promptCategoryTabs)
    {
        window->m_promptCategoryTabs->setStyleSheet(getTabStyle(grayTheme));
        
        // 更新列表样式（与导航栏选中样式一致）
        QString selectedBg = navChecked;  // 使用导航栏的选中背景色
        QString selectedText = grayTheme ? "#ffffff" : "#000000";  // 与导航栏选中文字一致
        QString hoverBg = navHover;  // 使用导航栏的悬停背景色
        
        QString scrollBarStyle = getScrollBarStyle(grayTheme);
        QString listStyle = getListStyle(grayTheme, selectedBg, selectedText, hoverBg);
        
        // 为所有列表设置滚动条样式
        for (QListWidget* list : window->m_promptTypeLists.values()) {
            if (list) {
                list->verticalScrollBar()->setStyleSheet(scrollBarStyle);
            }
        }
        
        for (QListWidget* list : window->m_promptTypeLists.values()) {
            if (list) {
                list->setStyleSheet(listStyle);
            }
        }
    }
    
    if (window->m_promptEdit)
    {
        window->m_promptEdit->setStyleSheet(getEditStyle(grayTheme));
    }

    // 结果文本区域样式
    if (window->m_resultText)
    {
        window->m_resultText->setStyleSheet(getResultTextStyle(grayTheme));
    }
    
    // 更新配置区域的标签样式（"模型"、"提示词"）
    auto allWidgets = window->m_sidebarPanel->findChildren<QWidget*>();
    for (QWidget* widget : allWidgets)
    {
        if (widget->property("configWidget").toBool())
        {
            auto labels = widget->findChildren<QLabel*>();
            QString style = getConfigLabelStyle(grayTheme);
            for (QLabel* lbl : labels)
            {
                lbl->setStyleSheet(style);
            }
            break;
        }
    }
    
    // 更新分隔线样式
    auto separators = window->findChildren<QFrame*>();
    QString sepStyle = getSeparatorStyle(grayTheme);
    for (QFrame* sep : separators)
    {
        if (sep->frameShape() == QFrame::HLine)
        {
            sep->setStyleSheet(sepStyle);
        }
    }
    
    // 更新折叠按钮样式
    if (window->m_sidebarToggleBtn)
    {
        QString toggleBtnColor = grayTheme ? "#b0b0b0" : "#666666";
        QString toggleBtnHoverBg = grayTheme ? "rgba(255, 255, 255, 0.1)" : "rgba(0, 0, 0, 0.05)";
        QString toggleBtnHoverColor = grayTheme ? "#ffffff" : "#000000";
        window->m_sidebarToggleBtn->setStyleSheet(getSidebarToggleStyle(toggleBtnColor, toggleBtnHoverBg, toggleBtnHoverColor));
    }
    
    // 更新 QSplitter 样式
    auto splitters = window->findChildren<QSplitter*>();
    QString handleColor = grayTheme ? "#3a3a3a" : "#ededed";
    QString handleBorder = grayTheme ? "#2a2a2a" : "#dcdcdc";
    QString handleHoverColor = grayTheme ? "#505050" : "#d8d8d8";
    QString handleHoverBorder = grayTheme ? "#3c3c3c" : "#c2c2c2";
    QString splitterStyle = getSplitterStyle(handleColor, handleBorder, handleHoverColor, handleHoverBorder);
    
    for (QSplitter* splitter : splitters)
    {
        splitter->setStyleSheet(splitterStyle);
        splitter->setHandleWidth(6);
        if (QSplitterHandle* handle = splitter->handle(1)) {
            handle->setCursor(Qt::SplitHCursor);
        }
    }
    
    // 更新图片显示区域样式
    if (window->m_imageLabel)
    {
        window->m_imageLabel->setStyleSheet(getImageLabelStyle(grayTheme));
    }
    
    // 更新历史记录列表样式
    if (window->m_historyList)
    {
        window->m_historyList->setStyleSheet(getHistoryListStyle(grayTheme));
    }
    
    // 更新状态栏样式
    window->statusBar()->setStyleSheet(getStatusBarStyle(grayTheme));
    
    // 更新滚动区域背景
    auto scrollAreas = window->findChildren<QScrollArea*>();
    for (QScrollArea* area : scrollAreas)
    {
        QString scrollAreaBg = grayTheme ? bg : "#f5f5f5";
        area->setStyleSheet(QString("QScrollArea { background-color: %1; border: none; }").arg(scrollAreaBg));
        if (area->viewport())
        {
            area->viewport()->setStyleSheet(QString("background-color: %1;").arg(scrollAreaBg));
        }
    }
    
    // 更新页面背景
    if (window->m_homePage)
    {
        QString pageBg = grayTheme ? bg : "#f5f5f5";
        window->m_homePage->setStyleSheet(QString("QWidget { background-color: %1; }").arg(pageBg));
    }
    if (window->m_historyPage)
    {
        QString pageBg = grayTheme ? bg : "#f5f5f5";
        window->m_historyPage->setStyleSheet(QString("QWidget { background-color: %1; }").arg(pageBg));
    }
    
    // 更新滚动条样式（增大尺寸以提高灵敏度）
    window->setStyleSheet(getScrollBarStyle(grayTheme));
    
    // 更新所有滚动区域的滚动步长
    auto scrollAreasForScroll = window->findChildren<QScrollArea*>();
    for (QScrollArea* area : scrollAreasForScroll)
    {
        area->verticalScrollBar()->setSingleStep(20);
        area->verticalScrollBar()->setPageStep(100);
        area->horizontalScrollBar()->setSingleStep(20);
        area->horizontalScrollBar()->setPageStep(100);
    }
    
    // 更新所有 QListWidget 和 QTextEdit 的滚动步长
    auto listWidgets = window->findChildren<QListWidget*>();
    for (QListWidget* list : listWidgets)
    {
        list->verticalScrollBar()->setSingleStep(20);
        list->verticalScrollBar()->setPageStep(100);
        list->horizontalScrollBar()->setSingleStep(20);
        list->horizontalScrollBar()->setPageStep(100);
    }
    
    auto textEdits = window->findChildren<QTextEdit*>();
    for (QTextEdit* edit : textEdits)
    {
        edit->verticalScrollBar()->setSingleStep(20);
        edit->verticalScrollBar()->setPageStep(100);
        edit->horizontalScrollBar()->setSingleStep(20);
        edit->horizontalScrollBar()->setPageStep(100);
    }
    
    // 更新配置区域标签颜色（通过查找配置区域的子标签）
    auto allWidgetsForConfig = window->m_sidebarPanel->findChildren<QWidget*>();
    for (QWidget* w : allWidgetsForConfig)
    {
        if (w->property("configWidget").toBool())
        {
            auto labels = w->findChildren<QLabel*>();
            QString labelColor = grayTheme ? "#b0b0b0" : "#666666";  // 更不透明的颜色
            for (QLabel* lbl : labels)
            {
                lbl->setStyleSheet(QString("QLabel { color: %1; font-size: 8pt; background: transparent; }").arg(labelColor));
            }
            break;
        }
    }
    
    // 更新滚动区域背景色
    auto scrollAreasForBg = window->findChildren<QScrollArea*>();
    for (QScrollArea* area : scrollAreasForBg)
    {
        area->setStyleSheet(QString("QScrollArea { background-color: %1; border: none; }").arg(bg));
    }
}

// 辅助方法实现

QString ThemeManager::getScrollBarStyle(bool grayTheme) {
    if (grayTheme) {
        return "QScrollBar:vertical { border: none; background: #2b2b2b; width: 18px; border-radius: 9px; }"
               "QScrollBar::handle:vertical { background: #555555; border-radius: 9px; min-height: 50px; margin: 2px; }"
               "QScrollBar::handle:vertical:hover { background: #666666; }"
               "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
               "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: #2b2b2b; }"
               "QScrollBar:horizontal { border: none; background: #2b2b2b; height: 18px; border-radius: 9px; }"
               "QScrollBar::handle:horizontal { background: #555555; border-radius: 9px; min-width: 50px; margin: 2px; }"
               "QScrollBar::handle:horizontal:hover { background: #666666; }"
               "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }"
               "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: #2b2b2b; }";
    } else {
        return "QScrollBar:vertical { border: none; background: #f5f5f5; width: 18px; border-radius: 9px; }"
               "QScrollBar::handle:vertical { background: #c0c0c0; border-radius: 9px; min-height: 50px; margin: 2px; }"
               "QScrollBar::handle:vertical:hover { background: #a0a0a0; }"
               "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
               "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: #f5f5f5; }"
               "QScrollBar:horizontal { border: none; background: #f5f5f5; height: 18px; border-radius: 9px; }"
               "QScrollBar::handle:horizontal { background: #c0c0c0; border-radius: 9px; min-width: 50px; margin: 2px; }"
               "QScrollBar::handle:horizontal:hover { background: #a0a0a0; }"
               "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }"
               "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: #f5f5f5; }";
    }
}

QString ThemeManager::getListStyle(bool grayTheme, const QString& selectedBg, const QString& selectedText, const QString& hoverBg) {
    if (grayTheme) {
        return QString("QListWidget { border: none; background: transparent; color: #e0e0e0; padding: 4px; }"
                       "QListWidget::item { padding: 8px 10px; border-bottom: 1px solid rgba(255, 255, 255, 0.1); border-radius: 4px; }"
                       "QListWidget::item:selected { background: %1; color: %2; }"
                       "QListWidget::item:hover { background: %3; }"
                       "QListWidget::item:selected:hover { background: %1; }")
               .arg(selectedBg).arg(selectedText).arg(hoverBg);
    } else {
        return QString("QListWidget { border: none; background: transparent; color: #000000; padding: 4px; }"
                       "QListWidget::item { padding: 8px 10px; border-bottom: 1px solid #f0f0f0; border-radius: 4px; }"
                       "QListWidget::item:selected { background: %1; color: %2; }"
                       "QListWidget::item:hover { background: %3; }"
                       "QListWidget::item:selected:hover { background: %1; }")
               .arg(selectedBg).arg(selectedText).arg(hoverBg);
    }
}

QString ThemeManager::getTabStyle(bool grayTheme) {
    if (grayTheme) {
        return "QTabWidget::pane { "
               "  border: 1px solid rgba(255, 255, 255, 0.2); "
               "  border-radius: 8px; "
               "  background: rgba(0, 0, 0, 0.2); "
               "}"
               "QTabBar::tab { "
               "  background: rgba(0, 0, 0, 0.1); "
               "  color: #b0b0b0; "
               "  padding: 6px 16px; "
               "  margin-right: 2px; "
               "  border-top-left-radius: 6px; "
               "  border-top-right-radius: 6px; "
               "  font-size: 9pt; "
               "}"
               "QTabBar::tab:selected { "
               "  background: rgba(0, 0, 0, 0.2); "
               "  color: #ffffff; "
               "  border-bottom: 2px solid #ffffff; "
               "}"
               "QTabBar::tab:hover { "
               "  background: rgba(0, 0, 0, 0.15); "
               "}"
               "QTabBar QToolButton { "
               "  background: rgba(0, 0, 0, 0.15); "
               "  border: 1px solid rgba(255, 255, 255, 0.2); "
               "  border-radius: 4px; "
               "  width: 24px; "
               "  height: 24px; "
               "}"
               "QTabBar QToolButton:hover { "
               "  background: rgba(255, 255, 255, 0.1); "
               "  border-color: rgba(255, 255, 255, 0.3); "
               "}"
               "QTabBar QToolButton:pressed { "
               "  background: rgba(255, 255, 255, 0.15); "
               "}"
               "QTabBar QToolButton::left-arrow { "
               "  image: none; "
               "  border-left: 5px solid transparent; "
               "  border-right: 5px solid transparent; "
               "  border-top: 6px solid #b0b0b0; "
               "  width: 0; "
               "  height: 0; "
               "  margin: 2px; "
               "}"
               "QTabBar QToolButton::left-arrow:hover { "
               "  border-top-color: #ffffff; "
               "}"
               "QTabBar QToolButton::right-arrow { "
               "  image: none; "
               "  border-left: 5px solid transparent; "
               "  border-right: 5px solid transparent; "
               "  border-bottom: 6px solid #b0b0b0; "
               "  width: 0; "
               "  height: 0; "
               "  margin: 2px; "
               "}"
               "QTabBar QToolButton::right-arrow:hover { "
               "  border-bottom-color: #ffffff; "
               "}";
    } else {
        return "QTabWidget::pane { "
               "  border: 1px solid #e0e0e0; "
               "  border-radius: 8px; "
               "  background: #ffffff; "
               "}"
               "QTabBar::tab { "
               "  background: #f5f5f5; "
               "  color: #666666; "
               "  padding: 6px 16px; "
               "  margin-right: 2px; "
               "  border-top-left-radius: 6px; "
               "  border-top-right-radius: 6px; "
               "  font-size: 9pt; "
               "}"
               "QTabBar::tab:selected { "
               "  background: #ffffff; "
               "  color: #000000; "
               "  border-bottom: 2px solid #000000; "
               "}"
               "QTabBar::tab:hover { "
               "  background: #eeeeee; "
               "}"
               "QTabBar QToolButton { "
               "  background: #f5f5f5; "
               "  border: 1px solid #e0e0e0; "
               "  border-radius: 4px; "
               "  width: 24px; "
               "  height: 24px; "
               "}"
               "QTabBar QToolButton:hover { "
               "  background: #eeeeee; "
               "  border-color: #cccccc; "
               "}"
               "QTabBar QToolButton:pressed { "
               "  background: #e0e0e0; "
               "}"
               "QTabBar QToolButton::left-arrow { "
               "  image: none; "
               "  border-left: 5px solid transparent; "
               "  border-right: 5px solid transparent; "
               "  border-top: 6px solid #666666; "
               "  width: 0; "
               "  height: 0; "
               "  margin: 2px; "
               "}"
               "QTabBar QToolButton::left-arrow:hover { "
               "  border-top-color: #000000; "
               "}"
               "QTabBar QToolButton::right-arrow { "
               "  image: none; "
               "  border-left: 5px solid transparent; "
               "  border-right: 5px solid transparent; "
               "  border-bottom: 6px solid #666666; "
               "  width: 0; "
               "  height: 0; "
               "  margin: 2px; "
               "}"
               "QTabBar QToolButton::right-arrow:hover { "
               "  border-bottom-color: #000000; "
               "}";
    }
}

QString ThemeManager::getComboBoxStyle(bool grayTheme) {
    if (grayTheme) {
        return "QComboBox { padding: 10px 14px; font-size: 10pt; font-weight: normal; border: 1px solid rgba(255, 255, 255, 0.2); border-radius: 8px; background: rgba(0, 0, 0, 0.2); color: #e0e0e0; }"
               "QComboBox:hover { background: rgba(0, 0, 0, 0.3); border-color: rgba(255, 255, 255, 0.3); color: #ffffff; }"
               "QComboBox::drop-down { border: none; width: 28px; }"
               "QComboBox::down-arrow { image: none; border-left: 4px solid transparent; border-right: 4px solid transparent; border-top: 5px solid #b0b0b0; width: 0; height: 0; }"
               "QComboBox QAbstractItemView { background: #2c2c2c; color: #e0e0e0; border: 1px solid rgba(255, 255, 255, 0.2); border-radius: 6px; padding: 4px; selection-background-color: #404040; selection-color: #ffffff; }";
    } else {
        return "QComboBox { padding: 10px 14px; font-size: 10pt; font-weight: normal; border: 1px solid #e0e0e0; border-radius: 8px; background: #ffffff; color: #000000; }"
               "QComboBox:hover { background: #f5f5f5; border-color: #ccc; color: #000000; }"
               "QComboBox::drop-down { border: none; width: 28px; }"
               "QComboBox::down-arrow { image: none; border-left: 4px solid transparent; border-right: 4px solid transparent; border-top: 5px solid #666666; width: 0; height: 0; }"
               "QComboBox QAbstractItemView { background: #ffffff; color: #000000; border: 1px solid #e0e0e0; border-radius: 6px; padding: 4px; selection-background-color: #f5f5f5; selection-color: #000000; }";
    }
}

QString ThemeManager::getMainButtonStyle(bool grayTheme) {
    if (grayTheme) {
        return "QPushButton { "
               "  padding: 12px 24px; "
               "  font-size: 11pt; "
               "  font-weight: normal; "
               "  color: #e0e0e0; "
               "  background: #404040; "
               "  border: 1px solid rgba(255, 255, 255, 0.2); "
               "  border-radius: 8px;"
               "}"
               "QPushButton:hover { "
               "  background: #4a4a4a; "
               "  border-color: rgba(255, 255, 255, 0.3);"
               "}"
               "QPushButton::icon { "
               "  margin-right: 8px;"
               "}";
    } else {
        return "QPushButton { "
               "  padding: 12px 24px; "
               "  font-size: 11pt; "
               "  font-weight: normal; "
               "  color: #333; "
               "  background: #f5f5f5; "
               "  border: 1px solid #e0e0e0; "
               "  border-radius: 8px;"
               "}"
               "QPushButton:hover { "
               "  background: #e8e8e8; "
               "  border-color: #d0d0d0;"
               "}"
               "QPushButton::icon { "
               "  margin-right: 8px;"
               "}";
    }
}

QString ThemeManager::getRecognizeButtonStyle(bool grayTheme) {
    if (grayTheme) {
        return "QPushButton { "
               "  padding: 12px 16px; "
               "  font-size: 11pt; "
               "  font-weight: normal; "
               "  color: #e0e0e0; "
               "  background: rgba(255, 255, 255, 0.08); "
               "  border: 1px solid rgba(255, 255, 255, 0.15); "
               "  border-radius: 8px;"
               "}"
               "QPushButton:hover:enabled { "
               "  background: rgba(255, 255, 255, 0.12); "
               "  border-color: rgba(255, 255, 255, 0.25);"
               "  color: #ffffff;"
               "}"
               "QPushButton:disabled { "
               "  background: rgba(0, 0, 0, 0.2); "
               "  color: #666; "
               "  border-color: rgba(255, 255, 255, 0.1);"
               "}"
               "QPushButton::icon { "
               "  margin-right: 8px;"
               "}";
    } else {
        return "QPushButton { "
               "  padding: 12px 16px; "
               "  font-size: 11pt; "
               "  font-weight: normal; "
               "  color: #333333; "
               "  background: #f5f5f5; "
               "  border: 1px solid #e0e0e0; "
               "  border-radius: 8px;"
               "}"
               "QPushButton:hover:enabled { "
               "  background: #e8e8e8; "
               "  border-color: #d0d0d0;"
               "  color: #000000;"
               "}"
               "QPushButton:disabled { "
               "  background: #f9f9f9; "
               "  color: #999999; "
               "  border-color: #e5e5e5;"
               "}"
               "QPushButton::icon { "
               "  margin-right: 8px;"
               "}";
    }
}

QString ThemeManager::getResultButtonStyle(bool grayTheme) {
    if (grayTheme) {
        return "QPushButton { "
               "  padding: 6px 6px; "
               "  font-size: 10pt; "
               "  font-weight: normal; "
               "  color: rgba(224, 224, 224, 0.7); "
               "  background: transparent; "
               "  border: none; "
               "}"
               "QPushButton:hover { "
               "  color: #e0e0e0; "
               "  background: transparent;"
               "}"
               "QPushButton::icon { "
               "  margin-right: 6px;"
               "}";
    } else {
        return "QPushButton { "
               "  padding: 6px 6px; "
               "  font-size: 10pt; "
               "  font-weight: normal; "
               "  color: #666; "
               "  background: transparent; "
               "  border: none; "
               "}"
               "QPushButton:hover { "
               "  color: #333; "
               "  background: transparent;"
               "}"
               "QPushButton::icon { "
               "  margin-right: 6px;"
               "}";
    }
}

QString ThemeManager::getBatchNavStyle(bool grayTheme) {
    if (grayTheme) {
        return "QPushButton { "
               "  background: rgba(255, 255, 255, 0.18); "
               "  color: #f5f5f5; "
               "  border: 1px solid rgba(255, 255, 255, 0.25); "
               "  border-radius: 10px; "
               "  font-size: 16pt; "
               "}"
               "QPushButton:hover { "
               "  background: rgba(255, 255, 255, 0.26); "
               "  border-color: rgba(255, 255, 255, 0.35); "
               "}"
               "QPushButton:pressed { "
               "  background: rgba(255, 255, 255, 0.32); "
               "}";
    } else {
        return "QPushButton { "
               "  background: rgba(255, 255, 255, 0.9); "
               "  color: #333; "
               "  border: 1px solid #d0d0d0; "
               "  border-radius: 10px; "
               "  font-size: 16pt; "
               "}"
               "QPushButton:hover { "
               "  background: rgba(255, 255, 255, 1); "
               "  border-color: #b0b0b0; "
               "}"
               "QPushButton:pressed { "
               "  background: rgba(245, 245, 245, 1); "
               "}";
    }
}

QString ThemeManager::getBatchInfoStyle(bool grayTheme) {
    return grayTheme
        ? "QLabel { background: rgba(0,0,0,0.35); color: #f5f5f5; padding: 4px 8px; border-radius: 8px; font-size: 9pt; }"
        : "QLabel { background: rgba(255,255,255,0.8); color: #000; padding: 4px 8px; border-radius: 8px; font-size: 9pt; }";
}

QString ThemeManager::getCloseButtonStyle(bool grayTheme) {
    if (grayTheme) {
        return "QPushButton { "
               "  background: rgba(255, 255, 255, 0.28); "
               "  color: #111; "
               "  border: 1px solid rgba(255, 255, 255, 0.35); "
               "  border-radius: 14px; "
               "  font-size: 18pt; "
               "  font-weight: bold; "
               "}"
               "QPushButton:hover { "
               "  background: rgba(255, 255, 255, 0.40); "
               "  border-color: rgba(255, 255, 255, 0.50); "
               "}";
    } else {
        return "QPushButton { "
               "  background: rgba(0, 0, 0, 0.6); "
               "  color: white; "
               "  border: none; "
               "  border-radius: 14px; "
               "  font-size: 18pt; "
               "  font-weight: bold; "
               "}"
               "QPushButton:hover { "
               "  background: rgba(220, 53, 69, 0.8); "
               "}";
    }
}

QString ThemeManager::getDarkButtonStyle(const QString& btnDark, const QString& btnDarkHover) {
    return QString(
        "QPushButton { "
        "  padding: 10px 20px; "
        "  font-size: 10pt; "
        "  font-weight: normal; "
        "  color: white; "
        "  background: %1; "
        "  border: none; "
        "  border-radius: 8px;"
        "}"
        "QPushButton:hover { "
        "  background: %2;"
        "}"
        "QPushButton:disabled { "
        "  background: #cccccc; "
        "  color: #888;"
        "}"
    )
    .arg(btnDark)
    .arg(btnDarkHover);
}

QString ThemeManager::getHintLabelStyle(bool grayTheme) {
    QString hintColor = grayTheme ? "rgba(200, 200, 200, 0.85)" : "rgba(100, 100, 100, 0.9)";
    return QString("QLabel { color: %1; background: transparent; border: none; font-size: 10pt; padding: 3px; margin: 0px; }").arg(hintColor);
}

QString ThemeManager::getEditStyle(bool grayTheme) {
    if (grayTheme) {
        return "QTextEdit { padding: 12px; font-size: 10pt; font-weight: 400; border: 1px solid rgba(255, 255, 255, 0.2); border-radius: 8px; background: rgba(0, 0, 0, 0.25); color: #e0e0e0; line-height: 1.5; }"
               "QTextEdit:focus { border-color: rgba(255, 255, 255, 0.35); background: rgba(0, 0, 0, 0.35); }";
    } else {
        return "QTextEdit { padding: 12px; font-size: 10pt; font-weight: 400; border: 1px solid #e0e0e0; border-radius: 8px; background: #ffffff; color: #000000; line-height: 1.5; }"
               "QTextEdit:focus { border-color: #000000; background: #ffffff; }";
    }
}

QString ThemeManager::getResultTextStyle(bool grayTheme) {
    if (grayTheme) {
        return "QTextEdit { padding: 15px; font-size: 11pt; border: 1px solid #3a3a3a; border-radius: 10px; background: #1f1f1f; color: #e0e0e0; line-height: 1.6; }";
    } else {
        return "QTextEdit { padding: 15px; font-size: 11pt; border: 2px solid #ddd; border-radius: 8px; background: white; color: #333; line-height: 1.6; }";
    }
}

QString ThemeManager::getConfigLabelStyle(bool grayTheme) {
    if (grayTheme) {
        return "QLabel { color: #b0b0b0; font-size: 10pt; font-weight: normal; background: transparent; margin-bottom: 6px; }";
    } else {
        return "QLabel { color: #666666; font-size: 10pt; font-weight: normal; background: transparent; margin-bottom: 6px; }";
    }
}

QString ThemeManager::getSeparatorStyle(bool grayTheme) {
    QString sepColor = grayTheme ? "rgba(255, 255, 255, 0.1)" : "rgba(0, 0, 0, 0.1)";
    return QString("QFrame { background: %1; max-height: 1px; }").arg(sepColor);
}

QString ThemeManager::getSidebarToggleStyle(const QString& color, const QString& hoverBg, const QString& hoverColor) {
    return QString(
        "QPushButton { "
        "  font-size: 14pt; "
        "  font-weight: normal; "
        "  color: %1; "
        "  background: transparent; "
        "  border: none; "
        "  border-radius: 4px; "
        "}"
        "QPushButton:hover { "
        "  background: %2; "
        "  color: %3;"
        "}"
    )
    .arg(color)
    .arg(hoverBg)
    .arg(hoverColor);
}

QString ThemeManager::getSplitterStyle(const QString& color, const QString& border, const QString& hoverColor, const QString& hoverBorder) {
    return QString(
        "QSplitter::handle:horizontal { "
        "  background: %1; "
        "  border-left: 1px solid %2; "
        "  border-right: 1px solid %2; "
        "}"
        "QSplitter::handle:horizontal:hover { "
        "  background: %3; "
        "  border-left: 1px solid %4; "
        "  border-right: 1px solid %4;"
        "}"
    )
    .arg(color)
    .arg(border)
    .arg(hoverColor)
    .arg(hoverBorder);
}

QString ThemeManager::getImageLabelStyle(bool grayTheme) {
    if (grayTheme) {
        return "QLabel { background: #3a3a3a; border: 3px dashed rgba(255, 255, 255, 0.2); border-radius: 12px; color: #999; font-size: 12pt; }";
    } else {
        return "QLabel { background: white; border: 3px dashed #ddd; border-radius: 12px; color: #999; font-size: 12pt; }";
    }
}

QString ThemeManager::getHistoryListStyle(bool grayTheme) {
    if (grayTheme) {
        return "QListWidget { background: #3a3a3a; border: 2px solid rgba(255, 255, 255, 0.2); border-radius: 12px; padding: 10px; font-size: 11pt; color: #e0e0e0; }"
               "QListWidget::item { padding: 15px; border-bottom: 1px solid rgba(255, 255, 255, 0.1); color: #e0e0e0; }"
               "QListWidget::item:hover { background: #404040; }"
               "QListWidget::item:selected { background: #505050; color: #ffffff; }";
    } else {
        return "QListWidget { background: white; border: 2px solid #ddd; border-radius: 12px; padding: 10px; font-size: 11pt; color: #333; }"
               "QListWidget::item { padding: 15px; border-bottom: 1px solid #eee; color: #333; }"
               "QListWidget::item:hover { background: #f5f5f5; }"
               "QListWidget::item:selected { background: #e0e0e0; color: #000000; }";
    }
}

QString ThemeManager::getStatusBarStyle(bool grayTheme) {
    if (grayTheme) {
        return "QStatusBar { background: #2c2c2c; border-top: 1px solid rgba(255, 255, 255, 0.1); color: #e0e0e0; }"
               "QStatusBar QLabel { color: #e0e0e0; }";
    } else {
        return "QStatusBar { background: white; border-top: 1px solid #ddd; color: #666; }"
               "QStatusBar QLabel { color: #666; }";
    }
}