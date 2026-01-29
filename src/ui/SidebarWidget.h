#pragma once
#include <QWidget>
#include <QPushButton>
#include <QComboBox>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>

// 侧边栏组件
class SidebarWidget : public QWidget {
    Q_OBJECT

public:
    explicit SidebarWidget(QWidget* parent = nullptr);
    ~SidebarWidget() override = default;

    // 获取 UI 元素
    QPushButton* themeToggleBtn() const { return m_themeToggleBtn; }
    QPushButton* sidebarToggleBtn() const { return m_sidebarToggleBtn; }
    
    QPushButton* navHomeBtn() const { return m_navHomeBtn; }
    QPushButton* navHistoryBtn() const { return m_navHistoryBtn; }
    QPushButton* navSettingsBtn() const { return m_navSettingsBtn; }
    
    // 配置区域元素获取器
    QComboBox* modelComboBox() const { return m_modelComboBox; }
    QTabWidget* promptTabs() const { return m_promptCategoryTabs; }
    QTextEdit* promptEdit() const { return m_promptEdit; }
    QPushButton* recognizeBtn() const { return m_recognizeBtn; }

    // 状态
    bool isCollapsed() const { return m_collapsed; }
    void setCollapsed(bool collapsed);
    
    // 主题
    void updateTheme(bool isGray);

signals:
    void collapseChanged(bool collapsed);

private:
    void setupUi();
    void updateCollapsedState();

    // UI 元素
    QPushButton* m_themeToggleBtn;
    QPushButton* m_sidebarToggleBtn;
    
    QPushButton* m_navHomeBtn;
    QPushButton* m_navHistoryBtn;
    QPushButton* m_navSettingsBtn;
    
    // 配置区域
    QWidget* m_configWidget;
    QVBoxLayout* m_configLayout;
    QComboBox* m_modelComboBox;
    QTabWidget* m_promptCategoryTabs;
    QTextEdit* m_promptEdit;
    QPushButton* m_recognizeBtn;
    
    bool m_collapsed;
    bool m_isGrayTheme;
};