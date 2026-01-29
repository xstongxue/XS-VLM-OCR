#include "SidebarWidget.h"
#include <QHBoxLayout>
#include <QIcon>
#include <QVariant>
#include <QDebug>
#include <QTransform>

SidebarWidget::SidebarWidget(QWidget* parent)
    : QWidget(parent)
    , m_collapsed(false)
    , m_isGrayTheme(false)
{
    setupUi();
}

void SidebarWidget::setupUi()
{
    setMinimumWidth(300);
    setMaximumWidth(QWIDGETSIZE_MAX);
    setStyleSheet("QWidget { background: #2c2c2c; }");

    QVBoxLayout* sidebarLayout = new QVBoxLayout(this);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(0);

    // 头部（主题切换 + 折叠按钮）
    QWidget* sidebarHeader = new QWidget();
    sidebarHeader->setObjectName("sidebarHeader");
    sidebarHeader->setFixedHeight(50);
    sidebarHeader->setStyleSheet("QWidget { background: transparent; }");
    QHBoxLayout* headerLayout = new QHBoxLayout(sidebarHeader);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(0);
    headerLayout->addStretch();

    // 主题切换按钮
    m_themeToggleBtn = new QPushButton();
    m_themeToggleBtn->setToolTip("切换主题");
    m_themeToggleBtn->setFixedSize(32, 32);
    m_themeToggleBtn->setIcon(QIcon(":/res/04.png"));
    m_themeToggleBtn->setIconSize(QSize(18, 18));
    m_themeToggleBtn->setStyleSheet(
        "QPushButton { padding: 0px; background: transparent; border: none; border-radius: 4px; }"
        "QPushButton:hover { background: rgba(255, 255, 255, 0.12); }"
    );
    headerLayout->addWidget(m_themeToggleBtn);

    // 折叠按钮
    m_sidebarToggleBtn = new QPushButton();
    m_sidebarToggleBtn->setToolTip("收起侧边栏");
    m_sidebarToggleBtn->setFixedSize(32, 32);
    m_sidebarToggleBtn->setStyleSheet(
        "QPushButton { font-size: 14pt; font-weight: normal; color: #b0b0b0; background: transparent; border: none; border-radius: 4px; padding: 0px; }"
        "QPushButton:hover { background: rgba(255, 255, 255, 0.1); color: #ffffff; }"
    );
    m_sidebarToggleBtn->setIcon(QIcon(":/res/sidebar.png"));
    m_sidebarToggleBtn->setIconSize(QSize(18, 18));
    headerLayout->addWidget(m_sidebarToggleBtn);
    headerLayout->setAlignment(m_sidebarToggleBtn, Qt::AlignRight | Qt::AlignVCenter);

    sidebarLayout->addWidget(sidebarHeader);

    // 导航按钮样式
    QString navBtnStyle = 
        "QPushButton { text-align: left; padding: 14px 20px; font-size: 11pt; font-weight: normal; color: #ffffff; border: none; background: transparent; border-radius: 0px; }"
        "QPushButton:hover { background: #3a3a3a; color: #ffffff; }"
        "QPushButton:checked { background: #404040; color: #ffffff; font-weight: normal; border-left: 4px solid #ffffff; }"
        "QPushButton::icon { margin-right: 12px; }";

    // 导航按钮
    m_navHomeBtn = new QPushButton("OCR识别");
    m_navHomeBtn->setCheckable(true);
    m_navHomeBtn->setChecked(true);
    m_navHomeBtn->setStyleSheet(navBtnStyle);
    m_navHomeBtn->setMinimumHeight(56);
    m_navHomeBtn->setProperty("originalText", "OCR识别");
    m_navHomeBtn->setIcon(QIcon(":/res/01.png"));
    m_navHomeBtn->setIconSize(QSize(22, 22));
    sidebarLayout->addWidget(m_navHomeBtn);

    m_navHistoryBtn = new QPushButton("历史记录");
    m_navHistoryBtn->setCheckable(true);
    m_navHistoryBtn->setStyleSheet(navBtnStyle);
    m_navHistoryBtn->setMinimumHeight(56);
    m_navHistoryBtn->setProperty("originalText", "历史记录");
    m_navHistoryBtn->setIcon(QIcon(":/res/02.png"));
    m_navHistoryBtn->setIconSize(QSize(22, 22));
    sidebarLayout->addWidget(m_navHistoryBtn);

    m_navSettingsBtn = new QPushButton("设置");
    m_navSettingsBtn->setCheckable(true);
    m_navSettingsBtn->setStyleSheet(navBtnStyle);
    m_navSettingsBtn->setMinimumHeight(56);
    m_navSettingsBtn->setProperty("originalText", "设置");
    m_navSettingsBtn->setIcon(QIcon(":/res/03.png"));
    m_navSettingsBtn->setIconSize(QSize(22, 22));
    sidebarLayout->addWidget(m_navSettingsBtn);

    // 配置区域
    m_configWidget = new QWidget();
    m_configWidget->setProperty("configWidget", true);
    m_configWidget->setStyleSheet("QWidget { background: transparent; }");
    m_configLayout = new QVBoxLayout(m_configWidget);
    m_configLayout->setContentsMargins(20, 16, 20, 16);
    m_configLayout->setSpacing(12);

    // 模型选择
    QLabel *modelLabel = new QLabel("模型");
    modelLabel->setStyleSheet("QLabel { color: #b0b0b0; font-size: 10pt; font-weight: normal; background: transparent; margin-bottom: 6px; }");
    m_configLayout->addWidget(modelLabel);

    m_modelComboBox = new QComboBox();
    m_modelComboBox->setMinimumHeight(40);
    // 样式表将在 updateTheme 中应用或在此处初始设置
    m_modelComboBox->setStyleSheet(
        "QComboBox { padding: 10px 14px; font-size: 10pt; font-weight: normal; border: 1px solid rgba(255, 255, 255, 0.2); border-radius: 8px; background: rgba(0, 0, 0, 0.2); color: #e0e0e0; }"
        "QComboBox:hover { background: rgba(0, 0, 0, 0.3); border-color: rgba(255, 255, 255, 0.3); }"
        "QComboBox::drop-down { border: none; width: 28px; }"
        "QComboBox::down-arrow { image: none; border-left: 4px solid transparent; border-right: 4px solid transparent; border-top: 5px solid #b0b0b0; width: 0; height: 0; }"
        "QComboBox QAbstractItemView { background: #2c2c2c; color: #e0e0e0; border: 1px solid rgba(255, 255, 255, 0.2); border-radius: 6px; padding: 4px; selection-background-color: #404040; selection-color: #ffffff; }"
    );
    m_configLayout->addWidget(m_modelComboBox);
    m_configLayout->addSpacing(8);

    // 提示词选项卡
    QLabel *promptLabel = new QLabel("提示词");
    promptLabel->setStyleSheet("QLabel { color: #b0b0b0; font-size: 10pt; font-weight: normal; background: transparent; margin-bottom: 6px; }");
    m_configLayout->addWidget(promptLabel);

    m_promptCategoryTabs = new QTabWidget();
    m_promptCategoryTabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_promptCategoryTabs->setMinimumHeight(320);
    m_promptCategoryTabs->setMaximumHeight(320);
    // 选项卡样式将在 updateTheme 中处理
    m_configLayout->addWidget(m_promptCategoryTabs, 1);

    // 提示词编辑框
    m_promptEdit = new QTextEdit();
    m_promptEdit->setMinimumHeight(90);
    m_promptEdit->setMaximumHeight(220);
    m_promptEdit->setPlaceholderText("自定义提示词...");
    m_promptEdit->setStyleSheet(
        "QTextEdit { padding: 12px; font-size: 10pt; font-weight: 400; border: 1px solid rgba(255, 255, 255, 0.2); border-radius: 8px; background: rgba(0, 0, 0, 0.2); color: #e0e0e0; line-height: 1.5; }"
        "QTextEdit:focus { border-color: rgba(255, 255, 255, 0.35); background: rgba(0, 0, 0, 0.3); }"
    );
    m_configLayout->addWidget(m_promptEdit, 2);

    // 识别按钮
    m_recognizeBtn = new QPushButton("询问AI");
    m_recognizeBtn->setIcon(QIcon(":/res/15.png"));
    m_recognizeBtn->setIconSize(QSize(18, 18));
    m_recognizeBtn->setMinimumHeight(44);
    m_recognizeBtn->setEnabled(true);
    m_recognizeBtn->setProperty("originalText", "询问AI");
    m_recognizeBtn->setStyleSheet(
        "QPushButton { padding: 12px 16px; font-size: 11pt; font-weight: normal; color: #e0e0e0; background: rgba(255, 255, 255, 0.08); border: 1px solid rgba(255, 255, 255, 0.15); border-radius: 8px; }"
        "QPushButton:hover:enabled { background: rgba(255, 255, 255, 0.12); border-color: rgba(255, 255, 255, 0.25); color: #ffffff; }"
        "QPushButton:disabled { background: rgba(0, 0, 0, 0.2); color: #666; border-color: rgba(255, 255, 255, 0.1); }"
        "QPushButton::icon { margin-right: 8px; }"
    );
    m_configLayout->addWidget(m_recognizeBtn);

    sidebarLayout->addWidget(m_configWidget);
    sidebarLayout->addStretch();
    
    // 连接折叠按钮
    connect(m_sidebarToggleBtn, &QPushButton::clicked, this, [this]() {
        setCollapsed(!m_collapsed);
    });
}

void SidebarWidget::setCollapsed(bool collapsed)
{
    if (m_collapsed == collapsed) return;
    m_collapsed = collapsed;
    updateCollapsedState();
    emit collapseChanged(m_collapsed);
}

void SidebarWidget::updateCollapsedState()
{
    if (m_collapsed) {
        // 折叠
        setMaximumWidth(60);
        setMinimumWidth(60);
        
        QTransform transform;
        transform.rotate(180);
        QPixmap pixmap = QIcon(":/res/sidebar.png").pixmap(16, 16);
        m_sidebarToggleBtn->setIcon(QIcon(pixmap.transformed(transform, Qt::SmoothTransformation)));
        m_sidebarToggleBtn->setToolTip("展开侧边栏");

        m_configWidget->hide();
        m_themeToggleBtn->hide();
        // 如果有分隔符或其他项（由布局/可见性管理），则隐藏
        
        // 将识别按钮移到侧边栏布局
        m_configLayout->removeWidget(m_recognizeBtn);
        layout()->addWidget(m_recognizeBtn); // 添加到主布局末尾
        
        // 折叠后的按钮样式
        QString collapsedBtnStyle = 
            "QPushButton { text-align: center; padding: 0px; font-size: 10pt; font-weight: normal; color: #ffffff; border: none; background: transparent; }"
            "QPushButton:hover:enabled { background: #3a3a3a; color: #ffffff; }"
            "QPushButton:hover:disabled { background: transparent; }"
            "QPushButton:checked { background: #404040; color: #ffffff; font-weight: normal; border-left: 3px solid #ffffff; }"
            "QPushButton:disabled { color: #666; }"
            "QPushButton::icon { margin-right: 0px; }";
            
        const int collapsedBtnSize = 48;
        
        auto setupCollapsedBtn = [&](QPushButton* btn) {
            btn->setStyleSheet(collapsedBtnStyle);
            btn->setText("");
            btn->setFixedSize(collapsedBtnSize, collapsedBtnSize);
            btn->setMinimumHeight(collapsedBtnSize);
            btn->setIconSize(QSize(24, 24));
        };
        
        setupCollapsedBtn(m_navHomeBtn);
        setupCollapsedBtn(m_navHistoryBtn);
        setupCollapsedBtn(m_navSettingsBtn);
        setupCollapsedBtn(m_recognizeBtn);
        
        m_recognizeBtn->show();
        
    } else {
        // 展开
        setMinimumWidth(300);
        setMaximumWidth(QWIDGETSIZE_MAX);
        
        m_sidebarToggleBtn->setIcon(QIcon(":/res/sidebar.png"));
        m_sidebarToggleBtn->setToolTip("收起侧边栏");
        
        m_configWidget->show();
        m_themeToggleBtn->show();
        
        // 将识别按钮移回
        layout()->removeWidget(m_recognizeBtn);
        m_configLayout->addWidget(m_recognizeBtn);
        
        // 恢复样式
        m_navHomeBtn->setText(m_navHomeBtn->property("originalText").toString());
        m_navHomeBtn->setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        m_navHomeBtn->setMinimumHeight(56);
        m_navHomeBtn->setIconSize(QSize(22, 22));
        
        m_navHistoryBtn->setText(m_navHistoryBtn->property("originalText").toString());
        m_navHistoryBtn->setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        m_navHistoryBtn->setMinimumHeight(56);
        m_navHistoryBtn->setIconSize(QSize(22, 22));
        
        m_navSettingsBtn->setText(m_navSettingsBtn->property("originalText").toString());
        m_navSettingsBtn->setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        m_navSettingsBtn->setMinimumHeight(56);
        m_navSettingsBtn->setIconSize(QSize(22, 22));
        
        m_recognizeBtn->setText(m_recognizeBtn->property("originalText").toString());
        m_recognizeBtn->setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        m_recognizeBtn->setMinimumHeight(44);
        m_recognizeBtn->setIconSize(QSize(18, 18));
        
        // 重新应用主题以确保颜色正确
        updateTheme(m_isGrayTheme);
    }
}

void SidebarWidget::updateTheme(bool isGray)
{
    m_isGrayTheme = isGray;
    
    // 选项卡样式
    QString tabStyle = isGray
        ? "QTabWidget::pane { border: 1px solid rgba(255, 255, 255, 0.2); border-radius: 8px; background: rgba(0, 0, 0, 0.2); }"
          "QTabBar::tab { background: rgba(0, 0, 0, 0.1); color: #b0b0b0; padding: 6px 16px; margin-right: 2px; border-top-left-radius: 6px; border-top-right-radius: 6px; font-size: 9pt; }"
          "QTabBar::tab:selected { background: rgba(0, 0, 0, 0.2); color: #ffffff; border-bottom: 2px solid #ffffff; }"
          "QTabBar::tab:hover { background: rgba(0, 0, 0, 0.15); }"
        : "QTabWidget::pane { border: 1px solid #e0e0e0; border-radius: 8px; background: #ffffff; }"
          "QTabBar::tab { background: #f5f5f5; color: #666666; padding: 6px 16px; margin-right: 2px; border-top-left-radius: 6px; border-top-right-radius: 6px; font-size: 9pt; }"
          "QTabBar::tab:selected { background: #ffffff; color: #000000; border-bottom: 2px solid #000000; }"
          "QTabBar::tab:hover { background: #eeeeee; }";
          
    m_promptCategoryTabs->setStyleSheet(tabStyle);
    
    // 识别按钮样式
    if (!m_collapsed) {
        QString mainBtnStyle = isGray
            ? "QPushButton { padding: 12px 16px; font-size: 11pt; font-weight: normal; color: #e0e0e0; background: rgba(255, 255, 255, 0.08); border: 1px solid rgba(255, 255, 255, 0.15); border-radius: 8px; }"
              "QPushButton:hover:enabled { background: rgba(255, 255, 255, 0.12); border-color: rgba(255, 255, 255, 0.25); color: #ffffff; }"
            : "QPushButton { padding: 12px 16px; font-size: 11pt; font-weight: normal; color: #333; background: #f5f5f5; border: 1px solid #e0e0e0; border-radius: 8px; }"
              "QPushButton:hover:enabled { background: #e0e0e0; border-color: #d0d0d0; color: #000; }";
        m_recognizeBtn->setStyleSheet(mainBtnStyle);
    }
    
    // 导航按钮样式更新
    QString navBtnStyle = 
        "QPushButton { text-align: left; padding: 14px 20px; font-size: 11pt; font-weight: normal; color: " + QString(isGray ? "#ffffff" : "#333333") + "; border: none; background: transparent; border-radius: 0px; }"
        "QPushButton:hover { background: " + QString(isGray ? "#3a3a3a" : "#e0e0e0") + "; color: " + QString(isGray ? "#ffffff" : "#000000") + "; }"
        "QPushButton:checked { background: " + QString(isGray ? "#404040" : "#d0d0d0") + "; color: " + QString(isGray ? "#ffffff" : "#000000") + "; font-weight: normal; border-left: 4px solid " + QString(isGray ? "#ffffff" : "#1b3864ff") + "; }"
        "QPushButton::icon { margin-right: 12px; }";

    if (!m_collapsed) {
        m_navHomeBtn->setStyleSheet(navBtnStyle);
        m_navHistoryBtn->setStyleSheet(navBtnStyle);
        m_navSettingsBtn->setStyleSheet(navBtnStyle);
    }
}