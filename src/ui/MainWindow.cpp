#include "MainWindow.h"
#include "SettingsDialog.h"
#include "../adapters/TesseractAdapter.h"
#include "../adapters/QwenAdapter.h"
#include "../adapters/CustomAdapter.h"
#include "../adapters/GLMAdapter.h"
#include "../adapters/PaddleAdapter.h"
#include "../adapters/DoubaoAdapter.h"
#include "../adapters/GeneralAdapter.h"
#include "../adapters/GeminiAdapter.h"
#include "../utils/ConfigManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QStackedWidget>
#include <QFrame>
#include <QSplitter>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QClipboard>
#include <QGuiApplication>
#include <QApplication>
#include <QScreen>
#include <QMenu>
#include <QAction>
#include <QDateTime>
#include <QDebug>
#include <QStatusBar>
#include <QTimer>
#include <QPalette>
#include <QScrollBar>
#include <QMouseEvent>
#include <QPainter>
#include <QRubberBand>
#include <QStyle>
#include <QIcon>
#include <QPixmap>
#include <QTransform>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTextBrowser>
#include <functional>
#include <QStandardItemModel>
#include <QStandardItem>

#ifdef _WIN32
#include <windows.h>
#include <QAbstractNativeEventFilter>
#endif

#ifdef _WIN32
int MainWindow::s_hotKeyIdCounter = 1000;
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_currentHistoryIndex(-1), m_recognizing(false), 
      m_screenshotShortcut(nullptr), m_recognizeShortcut(nullptr), 
      m_sidebarCollapsed(false), m_mainSplitter(nullptr),
      m_trayNotified(false)
{
#ifdef _WIN32
    m_screenshotHotKeyId = 0;
    m_recognizeHotKeyId = 0;
#endif
    m_isGrayTheme = false;
    setWindowTitle("XS-VLM-OCR - 大模型时代的OCR工具");
    resize(1200, 800);
    
    // 设置窗口图标
    QIcon windowIcon(":/res/favicon.ico");
    if (!windowIcon.isNull()) {
        setWindowIcon(windowIcon);
    }

    // 启用拖拽
    setAcceptDrops(true);

    setupUI();
    initializeServices();
    setupConnections();
    setupShortcuts();
    setupSystemTray();

    showStatusMessage("就绪");
}

MainWindow::~MainWindow()
{
#ifdef _WIN32
    // 取消注册Windows全局热键
    if (m_screenshotHotKeyId != 0) {
        UnregisterHotKey((HWND)winId(), m_screenshotHotKeyId);
    }
    if (m_recognizeHotKeyId != 0) {
        UnregisterHotKey((HWND)winId(), m_recognizeHotKeyId);
    }
#endif
    
    delete m_modelManager;
    delete m_pipeline;
    delete m_clipboardManager;
    delete m_configManager;
}

QWidget* MainWindow::createCard(const QString& title, QWidget* content)
{
    QWidget* card = new QWidget();
    card->setProperty("themeCard", true);
    
    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 20, 20, 20);
    cardLayout->setSpacing(15);
    
    if (!title.isEmpty()) {
        QLabel* titleLabel = new QLabel(title);
        titleLabel->setStyleSheet(
            "QLabel { "
            "  font-size: 16pt; "
            "  font-weight: normal; "
            "  color: #333333; "
            "  background: transparent;"
            "}"
        );
        cardLayout->addWidget(titleLabel);
    }
    
    if (content) {
        cardLayout->addWidget(content);
    }
    
    return card;
}

void MainWindow::setupUI()
{
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);

    QHBoxLayout *mainLayout = new QHBoxLayout(m_centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ============ 左侧：导航栏（可伸缩）============
    m_sidebarPanel = new QWidget();
    m_sidebarPanel->setMinimumWidth(300); // 设置侧边栏最小宽度为300
    m_sidebarPanel->setMaximumWidth(QWIDGETSIZE_MAX); // 设置侧边栏最大宽度为 无限制
    m_sidebarPanel->setStyleSheet(
        "QWidget { "
        "  background: #2c2c2c;"
        "}"
    );
    
    QVBoxLayout *sidebarLayout = new QVBoxLayout(m_sidebarPanel);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(0);
    
    // 创建一个容器来放置折叠按钮（右上角）
    QWidget* sidebarHeader = new QWidget();
    sidebarHeader->setObjectName("sidebarHeader");
    sidebarHeader->setFixedHeight(50);
    sidebarHeader->setStyleSheet("QWidget { background: transparent; }");
    QHBoxLayout* headerLayout = new QHBoxLayout(sidebarHeader);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(0);
    headerLayout->addStretch();
    
    // 主题切换按钮（与折叠按钮同一行）
    m_themeToggleBtn = new QPushButton();
    m_themeToggleBtn->setToolTip("切换主题");
    m_themeToggleBtn->setFixedSize(32, 32);
    QIcon themeIcon(":/res/04.png");
    m_themeToggleBtn->setIcon(themeIcon);
    m_themeToggleBtn->setIconSize(QSize(18, 18));
    m_themeToggleBtn->setStyleSheet(
        "QPushButton { "
        "  padding: 0px; "
        "  background: transparent; "
        "  border: none; "
        "  border-radius: 4px; "
        "}"
        "QPushButton:hover { "
        "  background: rgba(255, 255, 255, 0.12); "
        "}"
    );
    headerLayout->addWidget(m_themeToggleBtn);
    
    // 折叠按钮（放在侧边栏右上角）
    m_sidebarToggleBtn = new QPushButton();
    m_sidebarToggleBtn->setToolTip("收起侧边栏");
    m_sidebarToggleBtn->setFixedSize(32, 32);
    m_sidebarToggleBtn->setStyleSheet(
        "QPushButton { "
        "  font-size: 14pt; "
        "  font-weight: normal; "
        "  color: #b0b0b0; "
        "  background: transparent; "
        "  border: none; "
        "  border-radius: 4px; "
        "  padding: 0px;"
        "}"
        "QPushButton:hover { "
        "  background: rgba(255, 255, 255, 0.1); "
        "  color: #ffffff;"
        "}"
    );
    QIcon sidebarIcon(":/res/sidebar.png");
    m_sidebarToggleBtn->setIcon(sidebarIcon);
    m_sidebarToggleBtn->setIconSize(QSize(18, 18));
    headerLayout->addWidget(m_sidebarToggleBtn);
    headerLayout->setAlignment(m_sidebarToggleBtn, Qt::AlignRight | Qt::AlignVCenter);
    
    sidebarLayout->addWidget(sidebarHeader);
    
    // 导航按钮样式（初始样式，会在主题切换时更新）
    QString navBtnStyle = 
        "QPushButton { "
        "  text-align: left; "
        "  padding: 14px 20px; "
        "  font-size: 11pt; "
        "  font-weight: normal; "
        "  color: #ffffff; "
        "  border: none; "
        "  background: transparent;"
        "  border-radius: 0px;"
        "}"
        "QPushButton:hover { "
        "  background: #3a3a3a; "
        "  color: #ffffff;"
        "}"
        "QPushButton:checked { "
        "  background: #404040; "
        "  color: #ffffff; "
        "  font-weight: normal; "
        "  border-left: 4px solid #ffffff;"
        "}"
        "QPushButton::icon { "
        "  margin-right: 12px;"
        "}";
    
    m_navHomeBtn = new QPushButton("OCR识别");
    m_navHomeBtn->setCheckable(true);
    m_navHomeBtn->setChecked(true);
    m_navHomeBtn->setStyleSheet(navBtnStyle);
    m_navHomeBtn->setMinimumHeight(56);
    m_navHomeBtn->setProperty("originalText", "OCR识别"); // 保存原始文字
    QIcon navHomeIcon(":/res/01.png");
    m_navHomeBtn->setIcon(navHomeIcon);
    m_navHomeBtn->setIconSize(QSize(22, 22));
    sidebarLayout->addWidget(m_navHomeBtn);
    
    m_navHistoryBtn = new QPushButton("历史记录");
    m_navHistoryBtn->setCheckable(true);
    m_navHistoryBtn->setStyleSheet(navBtnStyle);
    m_navHistoryBtn->setMinimumHeight(56);
    m_navHistoryBtn->setProperty("originalText", "历史记录"); // 保存原始文字
    QIcon navHistoryIcon(":/res/02.png");
    m_navHistoryBtn->setIcon(navHistoryIcon);
    m_navHistoryBtn->setIconSize(QSize(22, 22));
    sidebarLayout->addWidget(m_navHistoryBtn);
    
    m_navSettingsBtn = new QPushButton("设置");
    m_navSettingsBtn->setCheckable(true);
    m_navSettingsBtn->setStyleSheet(navBtnStyle);
    m_navSettingsBtn->setMinimumHeight(56);
    m_navSettingsBtn->setProperty("originalText", "设置"); // 保存原始文字
    QIcon navSettingsIcon(":/res/03.png");
    m_navSettingsBtn->setIcon(navSettingsIcon);
    m_navSettingsBtn->setIconSize(QSize(22, 22));
    sidebarLayout->addWidget(m_navSettingsBtn);
    
    // 分隔线
    // QFrame *separator1 = new QFrame();
    // separator1->setFrameShape(QFrame::HLine);
    // separator1->setFrameShadow(QFrame::Sunken);
    // separator1->setStyleSheet("QFrame { background: rgba(255, 255, 255, 0.1); max-height: 1px; }");
    // sidebarLayout->addWidget(separator1);
    
    // -------- 模型和提示词配置区域（紧凑布局）--------
    QWidget *configWidget = new QWidget();
    configWidget->setProperty("configWidget", true); // 标记为配置区域
    configWidget->setStyleSheet("QWidget { background: transparent; }");
    QVBoxLayout *configLayout = new QVBoxLayout(configWidget);
    configLayout->setContentsMargins(20, 16, 20, 16);
    configLayout->setSpacing(12);
    
    // 模型选择（紧凑）
    QLabel *modelLabel = new QLabel("模型");
    modelLabel->setStyleSheet("QLabel { color: #b0b0b0; font-size: 10pt; font-weight: normal; background: transparent; margin-bottom: 6px; }");
    configLayout->addWidget(modelLabel);
    
    m_modelComboBox = new QComboBox();
    m_modelComboBox->setMinimumHeight(40);
    m_modelComboBox->setStyleSheet(
        "QComboBox { "
        "  padding: 10px 14px; "
        "  font-size: 10pt; "
        "  font-weight: normal; "
        "  border: 1px solid rgba(255, 255, 255, 0.2); "
        "  border-radius: 8px; "
        "  background: rgba(0, 0, 0, 0.2); "
        "  color: #e0e0e0;"
        "}"
        "QComboBox:hover { "
        "  background: rgba(0, 0, 0, 0.3); "
        "  border-color: rgba(255, 255, 255, 0.3);"
        "}"
        "QComboBox::drop-down { "
        "  border: none; "
        "  width: 28px;"
        "}"
        "QComboBox::down-arrow { "
        "  image: none; "
        "  border-left: 4px solid transparent; "
        "  border-right: 4px solid transparent; "
        "  border-top: 5px solid #b0b0b0; "
        "  width: 0; "
        "  height: 0;"
        "}"
        "QComboBox QAbstractItemView { "
        "  background: #2c2c2c; "
        "  color: #e0e0e0; "
        "  border: 1px solid rgba(255, 255, 255, 0.2); "
        "  border-radius: 6px; "
        "  padding: 4px; "
        "  selection-background-color: #404040; "
        "  selection-color: #ffffff;"
        "}"
    );
    configLayout->addWidget(m_modelComboBox);
    
    configLayout->addSpacing(8);
    
    // 提示词模板选择（使用选项卡）
    QLabel *promptLabel = new QLabel("提示词");
    promptLabel->setStyleSheet("QLabel { color: #b0b0b0; font-size: 10pt; font-weight: normal; background: transparent; margin-bottom: 6px; }");
    configLayout->addWidget(promptLabel);
    
    // 创建提示词分类选项卡
    m_promptCategoryTabs = new QTabWidget();
    // 让提示词区域可充分显示，避免主题切换后显得数量变少
    m_promptCategoryTabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_promptCategoryTabs->setMinimumHeight(280);   // 提升最小高度，减少“少了很多”的错觉
    // 初始样式（会在主题切换时更新，这里使用默认主题）
    bool grayTheme = m_isGrayTheme;
    QString tabStyle = grayTheme
        ? "QTabWidget::pane { "
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
          "  background: rgba(255, 255, 255, 0.06); "
          "  border: 1px solid rgba(255, 255, 255, 0.18); "
          "  border-radius: 6px; "
          "  width: 22px; "
          "  height: 22px; "
          "  padding: 2px; "
          "  margin: 2px; "
          "}"
          "QTabBar QToolButton:hover { "
          "  background: rgba(255, 255, 255, 0.12); "
          "  border-color: rgba(255, 255, 255, 0.28); "
          "}"
          "QTabBar QToolButton:pressed { "
          "  background: rgba(255, 255, 255, 0.18); "
          "}"
          "QTabBar QToolButton::left-arrow { "
          "  image: none; "
          "  border-left: 5px solid transparent; "
          "  border-right: 5px solid transparent; "
          "  border-top: 6px solid #b0b0b0; "
          "  width: 0; "
          "  height: 0; "
          "  margin: 1px; "
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
          "  margin: 1px; "
          "}"
          "QTabBar QToolButton::right-arrow:hover { "
          "  border-bottom-color: #ffffff; "
          "}"
        : "QTabWidget::pane { "
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
          "  background: #f7f7f7; "
          "  border: 1px solid #dcdcdc; "
          "  border-radius: 6px; "
          "  width: 22px; "
          "  height: 22px; "
          "  padding: 2px; "
          "  margin: 2px; "
          "}"
          "QTabBar QToolButton:hover { "
          "  background: #f0f0f0; "
          "  border-color: #c9c9c9; "
          "}"
          "QTabBar QToolButton:pressed { "
          "  background: #e6e6e6; "
          "}"
          "QTabBar QToolButton::left-arrow { "
          "  image: none; "
          "  border-left: 5px solid transparent; "
          "  border-right: 5px solid transparent; "
          "  border-top: 6px solid #666666; "
          "  width: 0; "
          "  height: 0; "
          "  margin: 1px; "
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
          "  margin: 1px; "
          "}"
          "QTabBar QToolButton::right-arrow:hover { "
          "  border-bottom-color: #000000; "
          "}";
    m_promptCategoryTabs->setStyleSheet(tabStyle);
    configLayout->addWidget(m_promptCategoryTabs);
    
    // 提示词编辑（可展开，支持多行）
    m_promptEdit = new QTextEdit();
    m_promptEdit->setMinimumHeight(90);
    m_promptEdit->setMaximumHeight(220);
    m_promptEdit->setPlaceholderText("自定义提示词...");
    m_promptEdit->setStyleSheet(
        "QTextEdit { "
        "  padding: 12px; "
        "  font-size: 10pt; "
        "  font-weight: 400; "
        "  border: 1px solid rgba(255, 255, 255, 0.2); "
        "  border-radius: 8px; "
        "  background: rgba(0, 0, 0, 0.2); "
        "  color: #e0e0e0;"
        "  line-height: 1.5;"
        "}"
        "QTextEdit:focus { "
        "  border-color: rgba(255, 255, 255, 0.35); "
        "  background: rgba(0, 0, 0, 0.3);"
        "}"
    );
    configLayout->addWidget(m_promptEdit, 2); // 添加伸缩因子，让它可以占用更多空间
    
    // 开始识别按钮（询问AI按钮）
    m_recognizeBtn = new QPushButton("询问AI");
    m_recognizeBtn->setIcon(QIcon(":/res/15.png"));
    m_recognizeBtn->setIconSize(QSize(18, 18));
    m_recognizeBtn->setMinimumHeight(44);
    m_recognizeBtn->setEnabled(true); // 初始启用，允许无图片时询问AI
    m_recognizeBtn->setProperty("originalText", "询问AI"); // 保存原始文字，用于折叠/展开
    // 初始样式（深色主题），会在主题切换时更新
    m_recognizeBtn->setStyleSheet(
        "QPushButton { "
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
        "}"
    );
    configLayout->addWidget(m_recognizeBtn);
    
    sidebarLayout->addWidget(configWidget);
    
    sidebarLayout->addStretch();

    // ============ 右侧：内容区 ============
    m_contentPanel = new QWidget();
    m_contentPanel->setStyleSheet(
        "QWidget { "
        "  background-color: #f5f5f5;"
        "}"
    );
    
    QVBoxLayout *contentPanelLayout = new QVBoxLayout(m_contentPanel);
    contentPanelLayout->setContentsMargins(0, 0, 0, 0);
    contentPanelLayout->setSpacing(0);
    
    // 使用 QStackedWidget 管理不同页面
    m_stackedWidget = new QStackedWidget();
    contentPanelLayout->addWidget(m_stackedWidget);
    
    // 使用 QSplitter 实现可伸缩侧边栏
    m_mainSplitter = new QSplitter(Qt::Horizontal);
    m_mainSplitter->addWidget(m_sidebarPanel);
    m_mainSplitter->addWidget(m_contentPanel);
    m_mainSplitter->setStretchFactor(0, 0); // 侧边栏不伸缩
    m_mainSplitter->setCollapsible(0, false); // 侧边栏不可折叠（通过按钮控制）
    m_mainSplitter->setStretchFactor(1, 1); // 内容区伸缩因子为1
    m_mainSplitter->setSizes({300, 900}); // 设置初始大小，侧边栏宽度300，内容区宽度900
    m_mainSplitter->setChildrenCollapsible(false); // 设置子控件不可折叠
    m_mainSplitter->setOpaqueResize(true); // 设置分割线可拖拽
    m_mainSplitter->setHandleWidth(6); // 设置分割线宽度
    if (QSplitterHandle* handle = m_mainSplitter->handle(1)) {
        handle->setCursor(Qt::SplitHCursor); // 设置分割线鼠标样式
        handle->setToolTip("拖拽调整侧边栏宽度"); // 设置分割线提示
    }
    m_mainSplitter->setStyleSheet(
        "QSplitter::handle:horizontal { "
        "  background: #ededed; "
        "  border-left: 1px solid #dcdcdc; "
        "  border-right: 1px solid #dcdcdc; "
        "}"
        "QSplitter::handle:horizontal:hover { "
        "  background: #d8d8d8; "
        "  border-left: 1px solid #c2c2c2; "
        "  border-right: 1px solid #c2c2c2;"
        "}"
    );
    
    mainLayout->addWidget(m_mainSplitter);
    
    // ============ 创建首页（使用滚动区域）============
    QScrollArea *homeScrollArea = new QScrollArea();
    homeScrollArea->setWidgetResizable(true);
    homeScrollArea->setFrameShape(QFrame::NoFrame);
    homeScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    homeScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    homeScrollArea->verticalScrollBar()->setSingleStep(20);  // 每次滚动20像素
    homeScrollArea->verticalScrollBar()->setPageStep(100);    // 每页滚动100像素
    homeScrollArea->horizontalScrollBar()->setSingleStep(20);
    homeScrollArea->horizontalScrollBar()->setPageStep(100);
    homeScrollArea->setStyleSheet("QScrollArea { background-color: #f5f5f5; border: none; }");
    
    m_homePage = new QWidget();
    m_homePage->setStyleSheet("QWidget { background-color: #f5f5f5; }");
    QVBoxLayout *homeLayout = new QVBoxLayout(m_homePage);
    homeLayout->setContentsMargins(30, 30, 30, 30);
    homeLayout->setSpacing(20);
    homeLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    
    // -------- 图片上传卡片 --------
    QWidget *uploadCardContent = new QWidget();
    QVBoxLayout *uploadCardLayout = new QVBoxLayout(uploadCardContent);
    uploadCardLayout->setContentsMargins(0, 0, 0, 0);
    uploadCardLayout->setSpacing(15);
    
    QHBoxLayout *uploadBtnLayout = new QHBoxLayout();
    
    m_uploadBtn = new QPushButton("上传图片");
    m_uploadBtn->setIcon(QIcon(":/res/13.png"));
    m_uploadBtn->setIconSize(QSize(20, 20));
    m_uploadBtn->setMinimumHeight(45);
    m_uploadBtn->setStyleSheet(
        "QPushButton { "
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
        "}"
    );
    uploadBtnLayout->addWidget(m_uploadBtn);

    m_pasteBtn = new QPushButton("粘贴图片");
    m_pasteBtn->setIcon(QIcon(":/res/14.png"));
    m_pasteBtn->setIconSize(QSize(20, 20));
    m_pasteBtn->setMinimumHeight(45);
    m_pasteBtn->setStyleSheet(m_uploadBtn->styleSheet());
    uploadBtnLayout->addWidget(m_pasteBtn);
    
    uploadCardLayout->addLayout(uploadBtnLayout);
    
    // 图片显示容器（包含图片和关闭按钮）
    m_imageContainer = new QWidget();
    m_imageContainer->setMinimumHeight(200);
    m_imageContainer->setMaximumHeight(400);
    QVBoxLayout* imageContainerLayout = new QVBoxLayout(m_imageContainer);
    imageContainerLayout->setContentsMargins(0, 0, 0, 0);
    imageContainerLayout->setSpacing(0);
    
    m_imageLabel = new QLabel("来源: 粘贴 / 截图 / 拖拽图片到此 / 点击上传按钮");
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setMinimumHeight(200);
    m_imageLabel->setMaximumHeight(400);
    m_imageLabel->setStyleSheet(
        "QLabel { "
        "  background: white; "
        "  border: 3px dashed #ddd; "
        "  border-radius: 12px; "
        "  color: #999; "
        "  font-size: 12pt;"
        "}"
    );
    m_imageLabel->setScaledContents(false);
    
    // 关闭按钮（浮动在右上角）
    m_closeImageBtn = new QPushButton("×");
    m_closeImageBtn->setFixedSize(28, 28);
    m_closeImageBtn->setStyleSheet(
        "QPushButton { "
        "  background: rgba(0, 0, 0, 0.6); "
        "  color: white; "
        "  border: none; "
        "  border-radius: 14px; "
        "  font-size: 18pt; "
        "  font-weight: bold; "
        "}"
        "QPushButton:hover { "
        "  background: rgba(220, 53, 69, 0.8); "
        "}"
    );
    m_closeImageBtn->setCursor(Qt::PointingHandCursor);
    m_closeImageBtn->hide(); // 初始隐藏，有图片时才显示
    m_closeImageBtn->raise(); // 确保按钮在最上层
    
    // 使用相对定位布局
    imageContainerLayout->addWidget(m_imageLabel);
    m_imageContainer->setLayout(imageContainerLayout);
    
    // 将关闭按钮添加到容器（使用绝对定位）
    m_closeImageBtn->setParent(m_imageContainer);
    
    uploadCardLayout->addWidget(m_imageContainer, 1);
    
    QWidget *uploadCard = createCard("", uploadCardContent);
    homeLayout->addWidget(uploadCard, 1);
    
    // -------- 识别结果卡片 --------
    QWidget *resultCardContent = new QWidget();
    QVBoxLayout *resultCardLayout = new QVBoxLayout(resultCardContent);
    resultCardLayout->setContentsMargins(0, 0, 0, 0);
    resultCardLayout->setSpacing(2); // 减小按钮行和文本框之间的间距

    // 结果区域操作栏（左侧提示 + 右侧操作按钮）
    QHBoxLayout *resultHeaderLayout = new QHBoxLayout();
    resultHeaderLayout->setContentsMargins(0, 6, 0, 6); // 收紧上下边距
    resultHeaderLayout->setSpacing(12);

    QLabel *resultHintLabel = new QLabel("响应结果将显示在下方...");
    resultHintLabel->setProperty("resultHint", true); // 标记为结果提示标签
    resultHintLabel->setStyleSheet(
        "QLabel { "
        "  color: rgba(100, 100, 100, 0.9); "
        "  font-size: 10pt; "
        "  background: transparent; "
        "  border: none; "
        "  padding: 3px; "
        "  margin: 0px; "
        "}"
    );
    resultHeaderLayout->addWidget(resultHintLabel);
    resultHeaderLayout->addStretch();

    m_previewResultBtn = new QPushButton("预览");
    m_previewResultBtn->setIcon(QIcon(":/res/18.png"));
    m_previewResultBtn->setIconSize(QSize(18, 18));
    m_previewResultBtn->setMinimumHeight(36);
    m_previewResultBtn->setStyleSheet(
        "QPushButton { "
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
        "  margin-right: 2px;"
        "}"
    );
    resultHeaderLayout->addWidget(m_previewResultBtn);

    m_copyResultBtn = new QPushButton("复制结果");
    m_copyResultBtn->setIcon(QIcon(":/res/16.png"));
    m_copyResultBtn->setIconSize(QSize(18, 18));
    m_copyResultBtn->setMinimumHeight(36);
    m_copyResultBtn->setStyleSheet(
        "QPushButton { "
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
        "  margin-right: 2px;"
        "}"
    );
    if (m_previewResultBtn)
        m_previewResultBtn->setStyleSheet(m_copyResultBtn->styleSheet());
    resultHeaderLayout->addWidget(m_copyResultBtn);
    
    m_exportBtn = new QPushButton("导出文件");
    m_exportBtn->setIcon(QIcon(":/res/17.png"));
    m_exportBtn->setIconSize(QSize(18, 18));
    m_exportBtn->setMinimumHeight(36);
    m_exportBtn->setStyleSheet(m_copyResultBtn->styleSheet());
    resultHeaderLayout->addWidget(m_exportBtn);
    
    resultCardLayout->addLayout(resultHeaderLayout);

    m_resultText = new QTextEdit();
    m_resultText->setReadOnly(true);
    m_resultText->setPlaceholderText("");
    m_resultText->setMinimumHeight(150); // 结果文本区域最小高度为150
    m_resultText->setMaximumHeight(300); // 结果文本区域最大高度为300
    m_resultText->setStyleSheet(
        "QTextEdit { "
        "  padding: 15px; "
        "  font-size: 11pt; "
        "  border: 2px solid #ddd; "
        "  border-radius: 8px; "
        "  background: white; "
        "  color: #333; "
        "  line-height: 1.6;"
        "}"
    );
    resultCardLayout->addWidget(m_resultText, 1);
    
    QWidget *resultCard = createCard("", resultCardContent);
    homeLayout->addWidget(resultCard);
    
    // 将首页设置到滚动区域并添加到 StackedWidget
    homeScrollArea->setWidget(m_homePage);
    m_stackedWidget->addWidget(homeScrollArea);
    
    // ============ 创建历史记录页面（使用滚动区域）============
    QScrollArea *historyScrollArea = new QScrollArea();
    historyScrollArea->setWidgetResizable(true);
    historyScrollArea->setFrameShape(QFrame::NoFrame);
    historyScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    historyScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    historyScrollArea->verticalScrollBar()->setSingleStep(20);  // 每次滚动20像素
    historyScrollArea->verticalScrollBar()->setPageStep(100);    // 每页滚动100像素
    historyScrollArea->horizontalScrollBar()->setSingleStep(20);
    historyScrollArea->horizontalScrollBar()->setPageStep(100);
    historyScrollArea->setStyleSheet("QScrollArea { background-color: #f5f5f5; border: none; }");
    
    m_historyPage = new QWidget();
    m_historyPage->setStyleSheet("QWidget { background-color: #f5f5f5; }");
    QVBoxLayout *historyPageLayout = new QVBoxLayout(m_historyPage);
    historyPageLayout->setContentsMargins(30, 30, 30, 30);
    historyPageLayout->setSpacing(20);
    
    m_historyList = new QListWidget();
    m_historyList->setStyleSheet(
        "QListWidget { "
        "  background: white; "
        "  border: 2px solid #ddd; "
        "  border-radius: 12px; "
        "  padding: 10px; "
        "  font-size: 11pt;"
        "}"
        "QListWidget::item { "
        "  padding: 15px; "
        "  border-bottom: 1px solid #eee;"
        "}"
        "QListWidget::item:hover { "
        "  background: #f5f5f5;"
        "}"
    );
    historyPageLayout->addWidget(m_historyList);
    
    m_clearHistoryBtn = new QPushButton("清空历史");
    m_clearHistoryBtn->setMinimumHeight(40);
    m_clearHistoryBtn->setStyleSheet(
        "QPushButton { "
        "  padding: 10px 20px; "
        "  font-size: 10pt; "
        "  font-weight: normal; "
        "  color: white; "
        "  background: #f44336; "
        "  border: none; "
        "  border-radius: 8px;"
        "}"
        "QPushButton:hover { "
        "  background: #d32f2f;"
        "}"
    );
    historyPageLayout->addWidget(m_clearHistoryBtn);
    
    // 将历史页面设置到滚动区域并添加到 StackedWidget
    historyScrollArea->setWidget(m_historyPage);
    m_stackedWidget->addWidget(historyScrollArea);
    
    // 默认显示首页
    m_stackedWidget->setCurrentIndex(0);
    
    // 内容面板已加入 m_mainSplitter，用于支持拖拽调整侧边栏宽度
    
    // 状态栏
    m_statusLabel = new QLabel("就绪");
    m_statusLabel->setStyleSheet("QLabel { color: #666; font-size: 10pt; }");
    statusBar()->addWidget(m_statusLabel);
    statusBar()->setStyleSheet("QStatusBar { background: white; border-top: 1px solid #ddd; }");
    
    // 应用全局滚动条样式（增大尺寸以提高灵敏度）
    QString scrollBarStyle = 
        "QScrollBar:vertical { "
        "  border: none; "
        "  background: #f0f0f0; "
        "  width: 18px; "
        "  border-radius: 9px;"
        "}"
        "QScrollBar::handle:vertical { "
        "  background: #c0c0c0; "
        "  border-radius: 9px; "
        "  min-height: 50px;"
        "  margin: 2px;"
        "}"
        "QScrollBar::handle:vertical:hover { "
        "  background: #a0a0a0;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { "
        "  height: 0px;"
        "}"
        "QScrollBar:horizontal { "
        "  border: none; "
        "  background: #f0f0f0; "
        "  height: 18px; "
        "  border-radius: 9px;"
        "}"
        "QScrollBar::handle:horizontal { "
        "  background: #c0c0c0; "
        "  border-radius: 9px; "
        "  min-width: 50px;"
        "  margin: 2px;"
        "}"
        "QScrollBar::handle:horizontal:hover { "
        "  background: #a0a0a0;"
        "}"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { "
        "  width: 0px;"
        "}";
    setStyleSheet(scrollBarStyle);
    
    // 应用默认主题
    applyTheme(m_isGrayTheme);
}

void MainWindow::switchToPage(const QString& page)
{
    // 取消所有导航按钮的选中状态
    m_navHomeBtn->setChecked(false);
    m_navHistoryBtn->setChecked(false);
    m_navSettingsBtn->setChecked(false);
    
    if (page == "home") {
        m_navHomeBtn->setChecked(true);
        m_stackedWidget->setCurrentIndex(0); // 首页在索引 0
    } else if (page == "history") {
        m_navHistoryBtn->setChecked(true);
        m_stackedWidget->setCurrentIndex(1); // 历史页面在索引 1
    } else if (page == "settings") {
        m_navSettingsBtn->setChecked(true);
        onSettingsClicked();
    }
}

void MainWindow::setupConnections()
{
    // 导航按钮连接
    connect(m_navHomeBtn, &QPushButton::clicked, [this]() { switchToPage("home"); });
    connect(m_navHistoryBtn, &QPushButton::clicked, [this]() { switchToPage("history"); });
    connect(m_navSettingsBtn, &QPushButton::clicked, [this]() { switchToPage("settings"); });
    connect(m_themeToggleBtn, &QPushButton::clicked, this, &MainWindow::onThemeToggleClicked);
    connect(m_sidebarToggleBtn, &QPushButton::clicked, this, &MainWindow::onSidebarToggleClicked);
    
    // UI 按钮连接
    connect(m_uploadBtn, &QPushButton::clicked, this, &MainWindow::onUploadImageClicked);
    connect(m_pasteBtn, &QPushButton::clicked, this, &MainWindow::onPasteImageClicked);
    connect(m_recognizeBtn, &QPushButton::clicked, this, &MainWindow::onRecognizeClicked);
    connect(m_closeImageBtn, &QPushButton::clicked, this, &MainWindow::onCloseImageClicked);
    connect(m_modelComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onModelChanged);
    // 提示词模板选择将在加载时连接
    connect(m_previewResultBtn, &QPushButton::clicked, this, &MainWindow::onPreviewResultClicked);
    connect(m_copyResultBtn, &QPushButton::clicked, [this]()
            {
        if (!m_resultText->toPlainText().isEmpty()) {
            m_clipboardManager->copyText(m_resultText->toPlainText());
            showStatusMessage("结果已复制到剪贴板");
        } });
    connect(m_exportBtn, &QPushButton::clicked, this, &MainWindow::onExportResultClicked);

    // 历史列表
    connect(m_historyList, &QListWidget::itemClicked, this, &MainWindow::onHistoryItemClicked);
    connect(m_clearHistoryBtn, &QPushButton::clicked, this, &MainWindow::onClearHistoryClicked);

    // Pipeline 连接
    connect(m_pipeline, &OCRPipeline::recognitionStarted,
            this, &MainWindow::onRecognitionStarted);
    connect(m_pipeline, &OCRPipeline::recognitionCompleted,
            this, &MainWindow::onRecognitionCompleted);
    connect(m_pipeline, &OCRPipeline::recognitionFailed,
            this, &MainWindow::onRecognitionFailed);
}

void MainWindow::setupSystemTray()
{
    // 检查系统是否支持系统托盘
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qWarning() << "系统不支持系统托盘";
        return;
    }
    
    m_trayIcon = new QSystemTrayIcon(this);
    // 使用应用程序图标作为系统托盘图标
    QIcon trayIcon(":/res/favicon.ico");
    if (trayIcon.isNull()) {
        // 如果图标加载失败，使用窗口图标
        trayIcon = windowIcon();
    }
    if (!trayIcon.isNull()) {
        m_trayIcon->setIcon(trayIcon);
    } else {
        // 最后的备用方案：创建简单图标
        QPixmap pixmap(16, 16);
        pixmap.fill(Qt::white);
        m_trayIcon->setIcon(QIcon(pixmap));
    }
    m_trayIcon->setToolTip("XS-VLM-OCR");

    m_trayMenu = new QMenu(this);
    QAction *showAction = m_trayMenu->addAction("显示主窗口");
    QAction *quitAction = m_trayMenu->addAction("退出");

    connect(showAction, &QAction::triggered, this, &QWidget::showNormal);
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
    connect(m_trayIcon, &QSystemTrayIcon::activated,
            this, &MainWindow::onTrayIconActivated);

    m_trayIcon->setContextMenu(m_trayMenu);
    m_trayIcon->show();
}

void MainWindow::initializeServices()
{
    // 创建管理器
    m_modelManager = new ModelManager(this);
    m_pipeline = new OCRPipeline(this);
    m_clipboardManager = new ClipboardManager(this);
    m_configManager = new ConfigManager(this);

    qDebug() << "=== Initializing XS-VLM-OCR Services ===";

    // 尝试从配置文件加载模型
    QString configPath = QDir::currentPath() + "/models_config.json";
    QString templatePath = QDir::currentPath() + "/models_config.json";
    bool configLoaded = false;

    // 如果配置文件不存在，但模板文件存在，则从模板复制
    if (!QFile::exists(configPath) && QFile::exists(templatePath))
    {
        qDebug() << "MainWindow: 首次运行，从模板创建配置文件";
        qDebug() << "   模板文件:" << templatePath;
        qDebug() << "   目标文件:" << configPath;
        
        if (QFile::copy(templatePath, configPath))
        {
            qDebug() << "MainWindow: 配置文件创建成功";
            QMessageBox::information(this, "欢迎使用", 
                "检测到首次运行，已为您创建默认配置！\n\n"
                "包含多个 Qwen 模型模板和提示词模板。\n"
                "请点击右上角 ⚙️ 设置按钮，填写您的 API Key。\n\n"
                "提示：所有模型都需要阿里云百炼 API Key");
        }
        else
        {
            qWarning() << "MainWindow: 配置文件创建失败";
            QMessageBox::warning(this, "警告", 
                "无法从模板创建配置文件，程序将使用内置默认配置");
        }
    }

    if (QFile::exists(configPath))
    {
        qDebug() << "MainWindow: 加载模型配置:" << configPath;
        configLoaded = m_configManager->loadConfig(configPath);
    }
    else
    {
        qDebug() << "MainWindow: 配置文件和模板都不存在，将使用内置默认配置";
    }

    if (configLoaded)
    {
        // 从配置文件创建模型
        QList<ModelConfig> configs = m_configManager->getModelConfigs();
        qDebug() << "MainWindow: 找到" << configs.size() << "个模型配置";
        
        // 检查是否有可用的 API Key
        for (const ModelConfig &config : configs)
        {
            if (!config.enabled)
            {
                qDebug() << "MainWindow: 跳过禁用模型:" << config.id;
                continue;
            }

            ModelAdapter *adapter = nullptr;

            // 根据 engine 类型创建适配器
            if (config.engine == "tesseract")
            {
                adapter = new TesseractAdapter(config, this);
            }
            else if (config.engine == "qwen")
            {
                adapter = new QwenAdapter(config, this);
            }
            else if (config.engine == "custom")
            {
                adapter = new CustomAdapter(config, this);
            }
            else if (config.engine == "gen")
            {
                adapter = new GeneralAdapter(config, this);
            }
            else if (config.engine == "gemini")
            {
                adapter = new GeminiAdapter(config, this);
            }
            else if (config.engine == "glm")
            {
                adapter = new GLMAdapter(config, this);
            }
            else if (config.engine == "paddle")
            {
                adapter = new PaddleAdapter(config, this);
            }
            else if (config.engine == "doubao")
            {
                adapter = new DoubaoAdapter(config, this);
            }
            else
            {
                qWarning() << "MainWindow: 未知引擎类型:" << config.engine;
                continue;
            }

            if (adapter)
            {
                m_modelManager->addModel(adapter);
                qDebug() << "MainWindow: 模型已添加:" << config.displayName;
            }
        }
    }
    else
    {
        // 配置文件不存在或加载失败，使用默认配置
        qDebug() << "MainWindow: 使用默认模型配置";

        // 添加 Tesseract 模型
        ModelConfig tesseractConfig;
        tesseractConfig.id = "tesseract_local";
        tesseractConfig.displayName = "Tesseract (离线)";
        tesseractConfig.type = "local";
        tesseractConfig.engine = "tesseract";
        tesseractConfig.params["lang"] = "chi_sim+eng";
        tesseractConfig.params["path"] = "C:/Program Files/Tesseract-OCR/tesseract.exe";

        TesseractAdapter *tesseractAdapter = new TesseractAdapter(tesseractConfig, this);
        m_modelManager->addModel(tesseractAdapter);

        // 添加 Qwen 在线模型
        ModelConfig qwenConfig;
        qwenConfig.id = "qwen_vl_plus";
        qwenConfig.displayName = "Qwen-VL-Plus (均衡)";
        qwenConfig.type = "online";
        qwenConfig.engine = "qwen";
        qwenConfig.params["model_name"] = "qwen-vl-plus";
        qwenConfig.params["api_key"] = ""; // 默认密钥
        qwenConfig.params["api_host"] = "https://dashscope.aliyuncs.com/compatible-mode";
        qwenConfig.params["deploy_type"] = "online";
        qwenConfig.params["temperature"] = "0.1";

        QwenAdapter *qwenAdapter = new QwenAdapter(qwenConfig, this);
        m_modelManager->addModel(qwenAdapter);

        // 添加 Qwen-VL-Max 模型（更高精度）
        ModelConfig qwenMaxConfig;
        qwenMaxConfig.id = "qwen_vl_max";
        qwenMaxConfig.displayName = "Qwen-VL-Max (免费)";
        qwenMaxConfig.type = "online";
        qwenMaxConfig.engine = "qwen";
        qwenMaxConfig.params["model_name"] = "qwen2-vl-2b-instruct";
        qwenMaxConfig.params["api_key"] = ""; // 默认密钥
        qwenMaxConfig.params["api_host"] = "https://dashscope.aliyuncs.com/compatible-mode";
        qwenMaxConfig.params["deploy_type"] = "online";
        qwenMaxConfig.params["temperature"] = "0.1";

        QwenAdapter *qwenMaxAdapter = new QwenAdapter(qwenMaxConfig, this);
        m_modelManager->addModel(qwenMaxAdapter);
    }

    // 优先将默认模型设为激活（若存在）
    const QString preferredModelId = "qwen2_vl_2b";
    if (m_modelManager->getModel(preferredModelId)) {
        m_modelManager->setActiveModel(preferredModelId);
    }

    // 初始化模型
    qDebug() << "MainWindow: 初始化模型...";
    m_modelManager->initializeAll();
    
    // 更新按钮 tooltip（包含快捷键）
    updateButtonTooltips();

    // 检查模型是否初始化成功
    ModelAdapter *activeModel = m_modelManager->getActiveModel();
    if (!activeModel)
    {
        QString errorMsg = "没有可用的模型！";
        showStatusMessage("错误: " + errorMsg);    // 启动阶段仅在状态栏提示
        qWarning() << "No active model available";
    }
    else if (!activeModel->isInitialized())
    {
        qWarning() << "当前激活模型未初始化:" << activeModel->config().displayName;
        
        // 尝试自动切换到已初始化的模型
        QList<ModelAdapter*> initializedModels = m_modelManager->getInitializedModels();
        if (!initializedModels.isEmpty()) {
            ModelAdapter* fallbackModel = initializedModels.first();
            m_modelManager->setActiveModel(fallbackModel->config().id);
            showStatusMessage(QString("已自动切换到可用模型: %1").arg(fallbackModel->config().displayName), 3000);
            qDebug() << "自动切换到已初始化模型:" << fallbackModel->config().displayName;
        } else {
            // 没有可用的已初始化模型
            showStatusMessage("警告: 当前模型未初始化，请在设置中检查配置", 5000);
        }
    }
    else
    {
        qDebug() << "当前激活模型:" << activeModel->config().displayName << "初始化成功";
        m_statusLabel->setText("就绪 - " + activeModel->config().displayName);
    }

    // 更新 UI 模型列表
    for (ModelAdapter *adapter : m_modelManager->getAllModels())
    {
        QString itemText = adapter->config().displayName;
        // 添加类型信息到显示文本中
        QString typeText = adapter->config().type == "local" ? "[离线]" : "[线上]";
        itemText += " " + typeText;
        if (!adapter->isInitialized())
        {
            itemText += " [未初始化]";
        }
        m_modelComboBox->addItem(itemText, adapter->config().id);
        int idx = m_modelComboBox->count() - 1;
        m_modelComboBox->setItemData(idx, itemText, Qt::ToolTipRole); // 悬浮显示完整信息
    }
    
    // 加载提示词模板到选项卡（按类型分组）
    while (m_promptCategoryTabs->count() > 0) {
        m_promptCategoryTabs->removeTab(0);
    }
    m_promptTypeLists.clear(); // 清空现有提示词模板列表
    
    QList<PromptTemplate> templates = m_configManager->getPromptTemplates();
    
    // 按类型分组
    QMap<QString, QList<PromptTemplate>> typeGroups;
    for (const PromptTemplate& tmpl : templates) {
        typeGroups[tmpl.type].append(tmpl);
    }
    
    // 定义类型顺序
    QStringList typeOrder = {"识别", "翻译", "解答", "整理"};
    
    // 创建列表样式
    bool grayTheme = m_configManager->getSetting("gray_theme", false).toBool();
    
    // 滚动条样式（与主题匹配）
    QString scrollBarStyle = grayTheme
        ? "QScrollBar:vertical { border: none; background: #2b2b2b; width: 12px; border-radius: 6px; }"
          "QScrollBar::handle:vertical { background: #555555; border-radius: 6px; min-height: 30px; margin: 2px; }"
          "QScrollBar::handle:vertical:hover { background: #666666; }"
          "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
          "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: #2b2b2b; }"
        : "QScrollBar:vertical { border: none; background: #f5f5f5; width: 12px; border-radius: 6px; }"
          "QScrollBar::handle:vertical { background: #c0c0c0; border-radius: 6px; min-height: 30px; margin: 2px; }"
          "QScrollBar::handle:vertical:hover { background: #a0a0a0; }"
          "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
          "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: #f5f5f5; }";
    
    QString listStyle = grayTheme
        ? "QListWidget { "
          "  border: none; "
          "  background: #2b2b2b; "
          "  color: #e0e0e0; "
          "  padding: 4px; "
          "}"
          "QListWidget::item { "
          "  padding: 10px 12px; "
          "  border-bottom: 1px solid rgba(255, 255, 255, 0.08); "
          "  border-radius: 4px; "
          "}"
          "QListWidget::item:selected { "
          "  background: rgba(255, 255, 255, 0.12); "
          "  color: #ffffff; "
          "}"
          "QListWidget::item:hover { "
          "  background: rgba(255, 255, 255, 0.08); "
          "}"
          "QListWidget::item:selected:hover { "
          "  background: rgba(255, 255, 255, 0.12); "
          "}"
        : "QListWidget { "
          "  border: none; "
          "  background: transparent; "
          "  color: #000000; "
          "  padding: 4px; "
          "}"
          "QListWidget::item { "
          "  padding: 8px 10px; "
          "  border-bottom: 1px solid #f0f0f0; "
          "  border-radius: 4px; "
          "}"
          "QListWidget::item:selected { "
          "  background: #e0e0e0; "
          "  color: #000000; "
          "}"
          "QListWidget::item:hover { "
          "  background: #f5f5f5; "
          "}"
          "QListWidget::item:selected:hover { "
          "  background: #e0e0e0; "
          "}";
    
    // 按类型顺序创建选项卡
    for (const QString& type : typeOrder) {
        if (typeGroups.contains(type) && !typeGroups[type].isEmpty()) {
            // 创建选项卡
            QWidget* tab = new QWidget();
            QVBoxLayout* layout = new QVBoxLayout(tab);
            layout->setContentsMargins(0, 0, 0, 0);
            
            QListWidget* list = new QListWidget();
            list->setStyleSheet(listStyle);
            list->verticalScrollBar()->setStyleSheet(scrollBarStyle);
            layout->addWidget(list);
            
            m_promptCategoryTabs->addTab(tab, type);
            m_promptTypeLists[type] = list;
            
            // 添加该类型下的所有模板
            for (const PromptTemplate& tmpl : typeGroups[type]) {
                QString displayName = QString("%1 [%2]").arg(tmpl.name, tmpl.category);
                QListWidgetItem* item = new QListWidgetItem(displayName);
                item->setData(Qt::UserRole, tmpl.content);
                item->setData(Qt::UserRole + 1, tmpl.name);
                list->addItem(item);
            }
            
            // 连接点击事件
            connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
                QString content = item->data(Qt::UserRole).toString();
                if (!content.isEmpty() && m_promptEdit) {
                    m_promptEdit->setPlainText(content);
                    qDebug() << "选择提示词模板:" << item->data(Qt::UserRole + 1).toString();
                }
            });
        }
    }
    
    // 处理其他未在typeOrder中的类型
    for (auto it = typeGroups.begin(); it != typeGroups.end(); ++it) {
        if (!typeOrder.contains(it.key()) && !it.value().isEmpty()) {
            QWidget* tab = new QWidget();
            QVBoxLayout* layout = new QVBoxLayout(tab);
            layout->setContentsMargins(0, 0, 0, 0);
            
            QListWidget* list = new QListWidget();
            list->setStyleSheet(listStyle);
            layout->addWidget(list);
            
            m_promptCategoryTabs->addTab(tab, it.key());
            m_promptTypeLists[it.key()] = list;
            
            for (const PromptTemplate& tmpl : it.value()) {
                QString displayName = QString("%1 [%2]").arg(tmpl.name, tmpl.category);
                QListWidgetItem* item = new QListWidgetItem(displayName);
                item->setData(Qt::UserRole, tmpl.content);
                item->setData(Qt::UserRole + 1, tmpl.name);
                list->addItem(item);
            }
            
            connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
                QString content = item->data(Qt::UserRole).toString();
                if (!content.isEmpty() && m_promptEdit) {
                    m_promptEdit->setPlainText(content);
                    qDebug() << "选择提示词模板:" << item->data(Qt::UserRole + 1).toString();
                }
            });
        }
    }
    
    // 默认选中第一个选项卡的第一个模板（优先"识别"选项卡的"通用识别"）
    if (m_promptTypeLists.contains("识别")) {
        QListWidget* list = m_promptTypeLists["识别"];
        m_promptCategoryTabs->setCurrentIndex(0);
        // 查找"通用识别"
        for (int i = 0; i < list->count(); i++) {
            QListWidgetItem* item = list->item(i);
            if (item && item->data(Qt::UserRole + 1).toString().contains("通用识别")) {
                list->setCurrentItem(item);
                QString content = item->data(Qt::UserRole).toString();
                if (!content.isEmpty() && m_promptEdit) {
                    m_promptEdit->setPlainText(content);
                }
                break;
            }
        }
        // 如果没找到"通用识别"，选中第一个
        if (list->count() > 0 && list->currentItem() == nullptr) {
            list->setCurrentItem(list->item(0));
            QString content = list->item(0)->data(Qt::UserRole).toString();
            if (!content.isEmpty() && m_promptEdit) {
                m_promptEdit->setPlainText(content);
            }
        }
    }

    // 设置当前模型
    if (m_modelManager->getActiveModel())
    {
        ModelAdapter *activeModel = m_modelManager->getActiveModel();
        m_pipeline->setCurrentAdapter(activeModel);
        
        // 设置下拉框的当前选中项
        QString activeModelId = activeModel->config().id;
        for (int i = 0; i < m_modelComboBox->count(); ++i)
        {
            if (m_modelComboBox->itemData(i).toString() == activeModelId)
            {
                m_modelComboBox->setCurrentIndex(i);
                break;
            }
        }
        
        // 更新状态栏显示模型信息（包含类型）
        QString typeText = activeModel->config().type == "local" ? "离线" : "线上";
        m_statusLabel->setText(QString("就绪 - %1 (%2)").arg(activeModel->config().displayName, typeText));
    }
    qDebug() << "=== Services initialized ===";
}

#ifdef _WIN32
// 辅助函数：将Qt键序列转换为Windows热键参数
bool parseKeySequence(const QString& keySeq, UINT& mod, UINT& vk)
{
    mod = 0;
    vk = 0;
    
    QStringList parts = keySeq.split('+', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return false;
    
    // 解析修饰键
    for (const QString& part : parts) {
        QString key = part.trimmed().toLower();
        if (key == "ctrl") {
            mod |= MOD_CONTROL;
        } else if (key == "shift") {
            mod |= MOD_SHIFT;
        } else if (key == "alt") {
            mod |= MOD_ALT;
        } else if (key == "meta" || key == "win") {
            mod |= MOD_WIN;
        } else {
            // 解析主键
            if (key.length() == 1) {
                // 单个字符键
                char ch = key[0].toUpper().toLatin1();
                if (ch >= 'A' && ch <= 'Z') {
                    vk = ch;
                } else if (ch >= '0' && ch <= '9') {
                    vk = ch;
                }
            } else {
                // 特殊键
                if (key == "f1") vk = VK_F1;
                else if (key == "f2") vk = VK_F2;
                else if (key == "f3") vk = VK_F3;
                else if (key == "f4") vk = VK_F4;
                else if (key == "f5") vk = VK_F5;
                else if (key == "f6") vk = VK_F6;
                else if (key == "f7") vk = VK_F7;
                else if (key == "f8") vk = VK_F8;
                else if (key == "f9") vk = VK_F9;
                else if (key == "f10") vk = VK_F10;
                else if (key == "f11") vk = VK_F11;
                else if (key == "f12") vk = VK_F12;
                else if (key == "space") vk = VK_SPACE;
                else if (key == "enter" || key == "return") vk = VK_RETURN;
                else if (key == "escape" || key == "esc") vk = VK_ESCAPE;
                else if (key == "tab") vk = VK_TAB;
                else if (key == "backspace") vk = VK_BACK;
                else if (key == "delete" || key == "del") vk = VK_DELETE;
                else if (key == "insert" || key == "ins") vk = VK_INSERT;
                else if (key == "home") vk = VK_HOME;
                else if (key == "end") vk = VK_END;
                else if (key == "pageup" || key == "pgup") vk = VK_PRIOR;
                else if (key == "pagedown" || key == "pgdown") vk = VK_NEXT;
                else if (key == "up") vk = VK_UP;
                else if (key == "down") vk = VK_DOWN;
                else if (key == "left") vk = VK_LEFT;
                else if (key == "right") vk = VK_RIGHT;
            }
        }
    }
    
    return (mod != 0 && vk != 0);
}
#endif

void MainWindow::setupShortcuts()
{
    // 检查配置管理器是否已初始化
    if (!m_configManager) {
        qWarning() << "ConfigManager 未初始化，无法设置快捷键";
        return;
    }
    
    // 删除旧的快捷键
    if (m_screenshotShortcut) {
        delete m_screenshotShortcut;
        m_screenshotShortcut = nullptr;
    }
    if (m_recognizeShortcut) {
        delete m_recognizeShortcut;
        m_recognizeShortcut = nullptr;
    }
    
#ifdef _WIN32
    // 取消注册旧的Windows全局热键
    if (m_screenshotHotKeyId != 0) {
        UnregisterHotKey((HWND)winId(), m_screenshotHotKeyId);
        m_screenshotHotKeyId = 0;
    }
    if (m_recognizeHotKeyId != 0) {
        UnregisterHotKey((HWND)winId(), m_recognizeHotKeyId);
        m_recognizeHotKeyId = 0;
    }
#endif
    
    // 从配置加载快捷键
    QString screenshotKey = m_configManager->getSetting("shortcut_screenshot", "Ctrl+R").toString();
    QString recognizeKey = m_configManager->getSetting("shortcut_recognize", "Ctrl+E").toString();
    
#ifdef _WIN32
    // 冲突校验：同一热键直接提示但不阻塞启动
    if (!screenshotKey.isEmpty() && screenshotKey == recognizeKey)
    {
        showStatusMessage("截图和开始识别快捷键不能相同，请在设置中修改", 5000);
        // 不阻塞程序启动，仅记录错误
        // QMessageBox::warning(this, "快捷键冲突", "截图和开始识别快捷键不能相同，请在设置中修改。");
        qWarning() << "快捷键冲突：截图与识别相同，跳过快捷键注册";
        return;
    }
    // 注册Windows全局热键（截图快捷键）
    if (!screenshotKey.isEmpty()) {
        UINT mod = 0, vk = 0;
        if (parseKeySequence(screenshotKey, mod, vk)) {
            m_screenshotHotKeyId = s_hotKeyIdCounter++;
            if (RegisterHotKey((HWND)winId(), m_screenshotHotKeyId, mod, vk)) {
                qDebug() << "注册Windows全局截图快捷键成功:" << screenshotKey << "ID:" << m_screenshotHotKeyId;
            } else {
                DWORD err = GetLastError();
                qWarning() << "注册Windows全局截图快捷键失败:" << screenshotKey << "错误码:" << err;
                showStatusMessage("截图快捷键注册失败，可能与其他程序冲突，请修改快捷键", 5000);
                // 不阻塞程序启动，仅记录错误
                // QMessageBox::warning(this, "快捷键注册失败",
                //                      QString("截图快捷键注册失败，可能与其他程序冲突。\n错误码: %1\n请在设置中修改为其他组合。").arg(err));
                m_screenshotHotKeyId = 0;
            }
        } else {
            qWarning() << "无法解析截图快捷键:" << screenshotKey;
            showStatusMessage("截图快捷键无效，请在设置中修改", 5000);
            // 不阻塞程序启动，仅记录错误
            // QMessageBox::warning(this, "快捷键无效", "截图快捷键无效，请在设置中修改。");
        }
    }
    
    // 注册Windows全局热键（识别快捷键）
    if (!recognizeKey.isEmpty()) {
        UINT mod = 0, vk = 0;
        if (parseKeySequence(recognizeKey, mod, vk)) {
            m_recognizeHotKeyId = s_hotKeyIdCounter++;
            if (RegisterHotKey((HWND)winId(), m_recognizeHotKeyId, mod, vk)) {
                qDebug() << "注册Windows全局识别快捷键成功:" << recognizeKey << "ID:" << m_recognizeHotKeyId;
            } else {
                DWORD err = GetLastError();
                qWarning() << "注册Windows全局识别快捷键失败:" << recognizeKey << "错误码:" << err;
                showStatusMessage("开始识别快捷键注册失败，可能与其他程序冲突，请修改快捷键", 5000);
                // 不阻塞程序启动，仅记录错误
                // QMessageBox::warning(this, "快捷键注册失败",
                //                      QString("开始识别快捷键注册失败，可能与其他程序冲突。\n错误码: %1\n请在设置中修改为其他组合。").arg(err));
                m_recognizeHotKeyId = 0;
            }
        } else {
            qWarning() << "无法解析识别快捷键:" << recognizeKey;
            showStatusMessage("开始识别快捷键无效，请在设置中修改", 5000);
            // 不阻塞程序启动，仅记录错误
            // QMessageBox::warning(this, "快捷键无效", "开始识别快捷键无效，请在设置中修改。");
        }
    }
#else
    // 非Windows平台使用Qt快捷键（需要窗口焦点）
    if (!screenshotKey.isEmpty() && screenshotKey == recognizeKey)
    {
        showStatusMessage("截图和开始识别快捷键不能相同，请在设置中修改", 5000);
        // 不阻塞程序启动，仅记录错误
        // QMessageBox::warning(this, "快捷键冲突", "截图和开始识别快捷键不能相同，请在设置中修改。");
        qWarning() << "快捷键冲突：截图与识别相同，跳过快捷键注册";
        return;
    }
    // 注册截图快捷键
    if (!screenshotKey.isEmpty()) {
        try {
            m_screenshotShortcut = new QShortcut(QKeySequence(screenshotKey), this);
            connect(m_screenshotShortcut, &QShortcut::activated, this, &MainWindow::onScreenshotShortcut);
            qDebug() << "注册截图快捷键:" << screenshotKey;
        } catch (...) {
            qWarning() << "注册截图快捷键失败:" << screenshotKey;
        }
    }
    
    // 注册识别快捷键
    if (!recognizeKey.isEmpty()) {
        try {
            m_recognizeShortcut = new QShortcut(QKeySequence(recognizeKey), this);
            connect(m_recognizeShortcut, &QShortcut::activated, this, &MainWindow::onRecognizeShortcut);
            qDebug() << "注册识别快捷键:" << recognizeKey;
        } catch (...) {
            qWarning() << "注册识别快捷键失败:" << recognizeKey;
        }
    }
#endif
    
    // 更新按钮 tooltip
    updateButtonTooltips();
}

void MainWindow::updateButtonTooltips()
{
    if (!m_configManager) {
        return;
    }
    
    // 从配置获取快捷键
    QString pasteKey = m_configManager->getSetting("shortcut_paste", "Ctrl+V").toString();
    QString recognizeKey = m_configManager->getSetting("shortcut_recognize", "Ctrl+E").toString();
    
    // 更新粘贴按钮 tooltip
    if (m_pasteBtn) {
        if (!pasteKey.isEmpty()) {
            m_pasteBtn->setToolTip(QString("粘贴图片 (%1)").arg(pasteKey));
        } else {
            m_pasteBtn->setToolTip("粘贴图片");
        }
    }
    
    // 更新开始识别按钮 tooltip
    if (m_recognizeBtn) {
        QString tooltipText = "询问AI";
        if (!recognizeKey.isEmpty()) {
            tooltipText += QString(" (%1)").arg(recognizeKey);
        }
        m_recognizeBtn->setToolTip(tooltipText);
    }
}

void MainWindow::disableShortcuts()
{
#ifdef _WIN32
    if (m_screenshotHotKeyId != 0) {
        UnregisterHotKey((HWND)winId(), m_screenshotHotKeyId);
        m_screenshotHotKeyId = 0;
    }
    if (m_recognizeHotKeyId != 0) {
        UnregisterHotKey((HWND)winId(), m_recognizeHotKeyId);
        m_recognizeHotKeyId = 0;
    }
#else
    if (m_screenshotShortcut) {
        delete m_screenshotShortcut;
        m_screenshotShortcut = nullptr;
    }
    if (m_recognizeShortcut) {
        delete m_recognizeShortcut;
        m_recognizeShortcut = nullptr;
    }
#endif
}

void MainWindow::loadImage(const QImage &image, SubmitSource source)
{
    if (image.isNull())
    {
        QMessageBox::warning(this, "错误", "无效的图像");
        return;
    }

    m_currentImage = image;

    // 清除之前的识别结果
    if (m_resultText)
    {
        m_resultText->clear();
    }

    // 显示图像（缩放以适应显示）
    QPixmap pixmap = QPixmap::fromImage(image);
    int maxWidth = 600;
    if (pixmap.width() > maxWidth)
    {
        pixmap = pixmap.scaledToWidth(maxWidth, Qt::SmoothTransformation);
    }
    m_imageLabel->setPixmap(pixmap);
    m_imageLabel->setText(""); // 清除提示文字
    
    // 显示关闭按钮并定位到右上角
    if (m_closeImageBtn)
    {
        QWidget* container = m_closeImageBtn->parentWidget();
        if (container)
        {
            m_closeImageBtn->show();
            // 延迟定位，确保容器大小已确定
            QTimer::singleShot(10, [this, container]() {
                if (m_closeImageBtn && container)
                {
                    m_closeImageBtn->move(container->width() - m_closeImageBtn->width() - 10, 10);
                    m_closeImageBtn->raise();
                }
            });
        }
    }

    // 识别按钮始终保持启用（即使没有图片也可以询问AI）
    // m_recognizeBtn->setEnabled(true); // 已移除，按钮始终启用

    QString sourceText;
    switch (source)
    {
    case SubmitSource::Upload:
        sourceText = "上传";
        break;
    case SubmitSource::Paste:
        sourceText = "粘贴";
        break;
    case SubmitSource::Shortcut:
        sourceText = "截图";
        break;
    case SubmitSource::DragDrop:
        sourceText = "拖拽";
        break;
    }

    showStatusMessage(QString("图像已加载 (%1) - %2x%3")
                          .arg(sourceText)
                          .arg(image.width())
                          .arg(image.height()));
}

void MainWindow::onCloseImageClicked()
{
    // 清除图片
    m_currentImage = QImage();
    
    // 恢复初始提示文字
    m_imageLabel->clear();
    m_imageLabel->setText("来源: 粘贴 / 截图 / 拖拽图片到此 / 点击上传按钮");
    
    // 隐藏关闭按钮
    if (m_closeImageBtn)
    {
        m_closeImageBtn->hide();
    }
    
    // 清除识别结果
    if (m_resultText)
    {
        m_resultText->clear();
    }
    
    showStatusMessage("图片已清除");
}

void MainWindow::updateCloseButtonPosition()
{
    if (m_closeImageBtn && m_imageContainer)
    {
        m_closeImageBtn->move(m_imageContainer->width() - m_closeImageBtn->width() - 10, 10);
        m_closeImageBtn->raise();
    }
}

void MainWindow::onUploadImageClicked()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "选择图片",
        QString(),
        "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif *.webp)");

    if (fileName.isEmpty())
    {
        return;
    }

    QImage image(fileName);
    if (image.isNull())
    {
        QMessageBox::warning(this, "错误", "无法加载图片文件");
        return;
    }

    loadImage(image, SubmitSource::Upload);
}

void MainWindow::onPasteImageClicked()
{
    if (!m_clipboardManager->hasImage())
    {
        QMessageBox::information(this, "提示", "剪贴板中没有图片");
        return;
    }

    QImage image = m_clipboardManager->getImage();
    loadImage(image, SubmitSource::Paste);
}

void MainWindow::onRecognizeClicked()
{
    if (!m_pipeline->currentAdapter())
    {
        QMessageBox::warning(this, "错误", "请先选择模型");
        return;
    }

    if (!m_pipeline->currentAdapter()->isInitialized())
    {
        QString modelName = m_modelComboBox->currentText();
        QString engine = m_pipeline->currentAdapter()->config().engine;
        QString errorMsg;
        
        if (engine == "tesseract") {
            errorMsg = QString("模型未初始化！\n模型：%1\n\n"
                             "请检查 Tesseract 是否正确安装：\n"
                             "1. 下载安装 Tesseract-OCR\n"
                             "2. 添加到系统 PATH\n"
                             "3. 或在设置中指定完整路径").arg(modelName);
        } 
        else if (engine == "qwen") 
        {
            errorMsg = QString("模型未初始化！\n模型：%1\n\n"
                             "可能的原因：\n"
                             "1. API Key 未配置或无效\n"
                             "2. 网络连接问题\n"
                             "3. API 地址配置错误\n\n"
                             "请点击设置按钮，在「模型配置」标签页填写 API Key").arg(modelName);
        } 
        else if (engine == "custom") 
        {
            errorMsg = QString("模型未初始化！\n模型：%1\n\n"
                             "可能的原因：\n"
                             "1. API Key 未配置或无效\n"
                             "2. 网络连接问题\n"
                             "3. API 地址配置错误\n\n"
                             "请点击设置按钮，在「模型配置」标签页填写 API Key").arg(modelName);
        } 
        else if (engine == "gen") 
        {
            errorMsg = QString("模型未初始化！\n模型：%1\n\n"
                             "可能的原因：\n"
                             "1. API Key 未配置或无效\n"
                             "2. 网络连接问题\n"
                             "3. API 地址配置错误\n\n"
                             "请点击设置按钮，在「模型配置」标签页填写 API Key").arg(modelName);
        }
        else if (engine == "gemini") 
        {
            errorMsg = QString("模型未初始化！\n模型：%1\n\n"
                             "可能的原因：\n"
                             "1. API Key 未配置或无效\n"
                             "2. 网络连接问题\n"
                             "3. API 地址配置错误\n\n"
                             "请点击设置按钮，在「模型配置」标签页填写 API Key").arg(modelName);
        }
        else if (engine == "glm") 
        {
            errorMsg = QString("模型未初始化！\n模型：%1\n\n"
                             "可能的原因：\n"
                             "1. API Key 未配置或无效\n"
                             "2. 网络连接问题\n"
                             "3. API 地址配置错误\n\n"
                             "请点击设置按钮，在「模型配置」标签页填写 API Key").arg(modelName);
        }
        else if (engine == "paddle")
        {
            errorMsg = QString("模型未初始化！\n模型：%1\n\n"
                             "可能的原因：\n"
                             "1. API Token 未配置或无效\n"
                             "2. API URL 未配置\n"
                             "3. 网络连接问题\n\n"
                             "请点击设置按钮，在「模型配置」标签页填写 API Token 和 API URL").arg(modelName);
        } 
        else if (engine == "doubao")
        {
            errorMsg = QString("模型未初始化！\n模型：%1\n\n"
                             "可能的原因：\n"
                             "1. API Key 未配置或无效\n"
                             "2. API URL 未配置\n"
                             "3. 网络连接问题\n\n"
                             "请点击设置按钮，在「模型配置」标签页填写 API Key 和 API URL").arg(modelName);
        } 
        else 
        {
            errorMsg = QString("模型未初始化！\n模型：%1\n\n请检查模型配置。").arg(modelName);
        }
        
        QMessageBox::warning(this, "错误", errorMsg);
        return;
    }

    qDebug() << "=== Starting OCR Recognition / AI Query ===";
    
    // 图片可以为空（用于纯文本AI询问）
    QImage imageToSubmit = m_currentImage;
    if (imageToSubmit.isNull())
    {
        qDebug() << "No image provided, will perform text-only AI query";
    }
    else
    {
        qDebug() << "Image size:" << imageToSubmit.size();
    }
    
    qDebug() << "Model:" << m_pipeline->currentAdapter()->config().displayName;

    QString prompt = m_promptEdit->toPlainText().trimmed();
    m_pipeline->submitImage(imageToSubmit, SubmitSource::Upload, prompt);
    
    // 重新应用当前主题样式，防止折叠/展开状态下按钮图标错位
    applyTheme(m_isGrayTheme);
}

void MainWindow::onClearHistoryClicked()
{
    if (m_history.isEmpty())
    {
        return;
    }

    auto reply = QMessageBox::question(
        this,
        "确认",
        "确定要清空所有历史记录吗？",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        m_history.clear();
        m_historyList->clear();
        m_currentHistoryIndex = -1;
        showStatusMessage("历史记录已清空");
    }
}

void MainWindow::onModelChanged(int index)
{
    QString modelId = m_modelComboBox->itemData(index).toString();
    m_modelManager->setActiveModel(modelId);

    ModelAdapter *adapter = m_modelManager->getActiveModel();
    if (adapter)
    {
        m_pipeline->setCurrentAdapter(adapter);
        
        // 更新状态栏显示模型信息（包含类型）
        QString typeText = adapter->config().type == "local" ? "离线" : "线上";
        showStatusMessage(QString("当前模型: %1 (%2)").arg(adapter->config().displayName, typeText));
    }
}

void MainWindow::onPreviewResultClicked()
{
    const QString currentText = m_resultText ? m_resultText->toPlainText() : QString();
    QDialog dialog(this);
    dialog.setWindowTitle("结果预览 (Markdown)");
    dialog.resize(960, 600);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    QSplitter* splitter = new QSplitter(Qt::Horizontal, &dialog);
    
    QTextEdit* editor = new QTextEdit(splitter);
    editor->setPlainText(currentText);
    editor->setLineWrapMode(QTextEdit::NoWrap);
    
    QTextBrowser* preview = new QTextBrowser(splitter);
    preview->setOpenExternalLinks(true);
    preview->setMarkdown(currentText);
    
    splitter->addWidget(editor);
    splitter->addWidget(preview);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({480, 480});
    
    layout->addWidget(splitter, 1);
    
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    
    QPushButton* applyBtn = new QPushButton("应用到结果", &dialog);
    QPushButton* copyBtn = new QPushButton("复制到剪贴板", &dialog);
    QPushButton* closeBtn = new QPushButton("关闭", &dialog);
    btnLayout->addWidget(applyBtn);
    btnLayout->addWidget(copyBtn);
    btnLayout->addWidget(closeBtn);
    layout->addLayout(btnLayout);
    
    connect(editor, &QTextEdit::textChanged, [&]() {
        preview->setMarkdown(editor->toPlainText());
    });
    
    connect(applyBtn, &QPushButton::clicked, [&]() {
        if (m_resultText)
            m_resultText->setPlainText(editor->toPlainText());
        dialog.accept();
    });
    connect(copyBtn, &QPushButton::clicked, [&]() {
        QClipboard* cb = QApplication::clipboard();
        cb->setText(editor->toPlainText());
        showStatusMessage("已复制到剪贴板");
    });
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    
    dialog.exec();
}

void MainWindow::onExportResultClicked()
{
    if (m_resultText->toPlainText().isEmpty())
    {
        QMessageBox::information(this, "提示", "没有可导出的内容");
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(
        this,
        "导出文本",
        QString("ocr_result_%1.txt").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")),
        "文本文件 (*.txt)");

    if (fileName.isEmpty())
    {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "错误", "无法写入文件");
        return;
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << m_resultText->toPlainText();
    file.close();

    showStatusMessage("结果已导出到: " + fileName);
}

void MainWindow::setRecognizeButtonText(const QString& text)
{
    if (!m_recognizeBtn) return;

    if (m_sidebarCollapsed)
    {
        // 折叠状态下仅显示图标，保持固定方形尺寸
        m_recognizeBtn->setText("");
        m_recognizeBtn->setIconSize(QSize(24, 24));
        const int collapsedBtnSize = 48;
        m_recognizeBtn->setFixedSize(collapsedBtnSize, collapsedBtnSize);
        m_recognizeBtn->setMinimumHeight(collapsedBtnSize);
    }
    else
    {
        m_recognizeBtn->setText(text);
        m_recognizeBtn->setIconSize(QSize(18, 18));
        m_recognizeBtn->setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        m_recognizeBtn->setMinimumHeight(44);
    }
}

void MainWindow::onRecognitionStarted(const QImage &image, SubmitSource source)
{
    Q_UNUSED(image);
    Q_UNUSED(source);

    qDebug() << "========================================";
    qDebug() << "UI: 识别任务开始";
    qDebug() << "========================================";

    m_recognizing = true;
    m_recognizeBtn->setEnabled(false);
    setRecognizeButtonText("执行中...");
    showStatusMessage("正在执行...");
}

void MainWindow::onRecognitionCompleted(const OCRResult &result, const QImage &image, SubmitSource source)
{
    m_recognizing = false;
    m_recognizeBtn->setEnabled(true);
    setRecognizeButtonText("询问AI");

    if (!result.success)
    {
        onRecognitionFailed(result.errorMessage, image, source);
        return;
    }

    // 添加到历史
    HistoryItem item;
    item.image = image;
    item.result = result;
    item.source = source;
    item.timestamp = result.timestamp;

    addHistoryItem(item);
    updateResultDisplay(item);

    // 自动复制结果
    m_clipboardManager->copyText(result.fullText);

    // 显示通知
    m_trayIcon->showMessage(
        "识别完成",
        QString("识别出 %1 个字符，耗时 %2ms")
            .arg(result.fullText.length())
            .arg(result.processingTimeMs),
        QSystemTrayIcon::Information,
        3000);

    showStatusMessage(QString("识别完成，耗时 %1ms").arg(result.processingTimeMs));
}

void MainWindow::onRecognitionFailed(const QString &error, const QImage &image, SubmitSource source)
{
    Q_UNUSED(image);
    Q_UNUSED(source);

    qDebug() << "========================================";
    qDebug() << "UI: 识别失败";
    qDebug() << "  - 错误:" << error;
    qDebug() << "========================================";

    m_recognizing = false;
    m_recognizeBtn->setEnabled(true);
    setRecognizeButtonText("询问AI");

    QMessageBox::warning(this, "识别失败", error);
    showStatusMessage("识别失败: " + error);
}

void MainWindow::addHistoryItem(const HistoryItem &item)
{
    m_history.append(item);

    QString sourceText;
    switch (item.source)
    {
    case SubmitSource::Upload:
        sourceText = "📁";
        break;
    case SubmitSource::Paste:
        sourceText = "📋";
        break;
    case SubmitSource::Shortcut:
        sourceText = "✂️";
        break;
    case SubmitSource::DragDrop:
        sourceText = "🎯";
        break;
    }

    QString itemText = QString("%1 %2 - %3字符 [%4]")
                           .arg(sourceText)
                           .arg(item.timestamp.toString("MM-dd HH:mm:ss"))
                           .arg(item.result.fullText.length())
                           .arg(item.result.modelName);

    QListWidgetItem *listItem = new QListWidgetItem(itemText);
    listItem->setData(Qt::UserRole, m_history.size() - 1);
    m_historyList->insertItem(0, listItem);

    // 自动选中最新项
    m_historyList->setCurrentItem(listItem);
}

void MainWindow::updateResultDisplay(const HistoryItem &item)
{
    // 显示图像
    QPixmap pixmap = QPixmap::fromImage(item.image);
    int maxWidth = 600;
    if (pixmap.width() > maxWidth)
    {
        pixmap = pixmap.scaledToWidth(maxWidth, Qt::SmoothTransformation);
    }
    m_imageLabel->setPixmap(pixmap);
    m_imageLabel->setText(""); // 清除提示文字
    
    // 显示关闭按钮并定位到右上角
    if (m_closeImageBtn && m_imageContainer)
    {
        m_closeImageBtn->show();
        // 延迟定位，确保容器大小已确定
        QTimer::singleShot(10, this, &MainWindow::updateCloseButtonPosition);
    }

    // 显示结果
    m_resultText->setPlainText(item.result.fullText);
}

void MainWindow::onHistoryItemClicked(QListWidgetItem *item)
{
    int index = item->data(Qt::UserRole).toInt();
    if (index >= 0 && index < m_history.size())
    {
        m_currentHistoryIndex = index;
        updateResultDisplay(m_history[index]);
        // 切换回首页以显示预览与结果区域
        switchToPage("home");
        showStatusMessage("已加载历史记录");
    }
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick)
    {
        showNormal();
        activateWindow();
    }
}

void MainWindow::showStatusMessage(const QString &message, int timeout)
{
    m_statusLabel->setText(message);
    statusBar()->showMessage(message, timeout);
}

void MainWindow::onSettingsClicked()
{
    qDebug() << "打开设置对话框";
    
    // 打开设置前临时禁用快捷键，避免在设置界面触发全局热键导致卡住
    disableShortcuts();

    SettingsDialog dialog(m_configManager, this);
    connect(&dialog, &SettingsDialog::settingsChanged, this, &MainWindow::onSettingsChanged);
    
    dialog.exec();

    // 关闭设置后重新注册快捷键
    setupShortcuts();
}

// onPromptTemplateSelected 函数已移除，改为在列表项点击时直接处理

void MainWindow::onSettingsChanged()
{
    qDebug() << "设置已更改，重新加载配置";
    
    // 保存当前选择的模型ID，以便重新加载后恢复
    QString currentModelId;
    if (ModelAdapter* currentActive = m_modelManager->getActiveModel()) {
        currentModelId = currentActive->config().id;
        qDebug() << "设置已更改 - 保存当前模型ID:" << currentModelId << "(" << currentActive->config().displayName << ")";
    } else {
        qDebug() << "设置已更改 - 当前没有激活的模型";
    }
    
    // 重新加载模型配置
    QString configPath = m_configManager->getConfigPath();
    if (!configPath.isEmpty()) {
        m_configManager->loadConfig(configPath);
    }
    
    // 清空现有模型
    QList<ModelAdapter*> oldModels = m_modelManager->getAllModels();
    for (ModelAdapter* adapter : oldModels) {
        m_modelManager->removeModel(adapter->config().id);
    }
    
    // 临时保存要恢复的模型ID（在添加模型前，因为addModel会自动激活第一个模型）
    QString modelIdToRestore = currentModelId;
    
    // 重新创建模型（注意：addModel会自动将第一个模型设为激活）
    QList<ModelConfig> configs = m_configManager->getModelConfigs();
    for (const ModelConfig& config : configs) {
        if (!config.enabled) {
            continue;
        }
        
        ModelAdapter* adapter = nullptr;
        if (config.engine == "tesseract") {
            adapter = new TesseractAdapter(config, this);
        } else if (config.engine == "qwen") {
            adapter = new QwenAdapter(config, this);
        } else if (config.engine == "custom") {
            adapter = new CustomAdapter(config, this);
        } else if (config.engine == "gen") {
            adapter = new GeneralAdapter(config, this);
        } else if (config.engine == "gemini") {
            adapter = new GeminiAdapter(config, this);
        } else if (config.engine == "glm") {
            adapter = new GLMAdapter(config, this);
        } else if (config.engine == "paddle") {
            adapter = new PaddleAdapter(config, this);
        } else if (config.engine == "doubao") {
            adapter = new DoubaoAdapter(config, this);
        }
        
        if (adapter) {
            m_modelManager->addModel(adapter);
        }
    }
    
    // 重新初始化模型
    m_modelManager->initializeAll();
    
    // 尝试恢复之前选择的模型，如果不存在则保持当前激活的模型（第一个模型）
    if (!modelIdToRestore.isEmpty()) {
        ModelAdapter* modelToRestore = m_modelManager->getModel(modelIdToRestore);
        if (modelToRestore) {
            // 如果之前的模型仍然存在，恢复它
            qDebug() << "尝试恢复之前选择的模型:" << modelIdToRestore << "(" << modelToRestore->config().displayName << ")";
            m_modelManager->setActiveModel(modelIdToRestore);
            qDebug() << "成功恢复之前选择的模型:" << modelIdToRestore;
        } else {
            // 如果之前的模型不存在了，保持当前激活的模型（通常是第一个）
            ModelAdapter* currentActive = m_modelManager->getActiveModel();
            if (currentActive) {
                qDebug() << "之前的模型不存在(" << modelIdToRestore << ")，保持当前激活模型:" << currentActive->config().id << "(" << currentActive->config().displayName << ")";
            } else {
                qWarning() << "之前的模型不存在，且当前没有激活的模型";
            }
        }
    } else {
        // 如果没有保存的模型ID，保持当前激活的模型（通常是第一个）
        ModelAdapter* currentActive = m_modelManager->getActiveModel();
        if (currentActive) {
            qDebug() << "没有保存的模型ID，保持当前激活模型:" << currentActive->config().id << "(" << currentActive->config().displayName << ")";
        }
    }
    
    // 更新 UI（临时断开信号连接，避免触发模型切换）
    disconnect(m_modelComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
               this, &MainWindow::onModelChanged);
    
    m_modelComboBox->clear();
    for (ModelAdapter* adapter : m_modelManager->getAllModels()) {
        QString itemText = adapter->config().displayName;
        if (!adapter->isInitialized()) {
            itemText += " [未初始化]";
        }
        m_modelComboBox->addItem(itemText, adapter->config().id);
        int idx = m_modelComboBox->count() - 1;
        m_modelComboBox->setItemData(idx, itemText, Qt::ToolTipRole); // 悬浮显示完整信息
    }
    
    // 同步当前激活模型到下拉框与 pipeline
    if (ModelAdapter* active = m_modelManager->getActiveModel()) {
        m_pipeline->setCurrentAdapter(active);
        const QString activeId = active->config().id;
        for (int i = 0; i < m_modelComboBox->count(); ++i) {
            if (m_modelComboBox->itemData(i).toString() == activeId) {
                m_modelComboBox->setCurrentIndex(i);
                break;
            }
        }
        QString typeText = active->config().type == "local" ? "离线" : "线上";
        m_statusLabel->setText(QString("就绪 - %1 (%2)").arg(active->config().displayName, typeText));
    }
    
    // 重新连接信号
    connect(m_modelComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onModelChanged);
    
    // 重新加载提示词模板选项卡
    // 清空现有选项卡
    while (m_promptCategoryTabs->count() > 0) {
        m_promptCategoryTabs->removeTab(0);
    }
    m_promptTypeLists.clear();
    
    QList<PromptTemplate> templates = m_configManager->getPromptTemplates();
    
    // 按类型分组
    QMap<QString, QList<PromptTemplate>> typeGroups;
    for (const PromptTemplate& tmpl : templates) {
        typeGroups[tmpl.type].append(tmpl);
    }
    
    // 定义类型顺序
    QStringList typeOrder = {"识别", "翻译", "解答", "整理"};
    
    // 创建列表样式（与导航栏选中样式一致）
    bool grayTheme = m_configManager->getSetting("gray_theme", false).toBool();
    QString selectedBg = grayTheme ? "#404040" : "#e0e0e0";  // 与导航栏选中背景一致
    QString selectedBorder = grayTheme ? "#ffffff" : "#000000";  // 与导航栏选中边框一致
    QString selectedText = grayTheme ? "#ffffff" : "#000000";  // 与导航栏选中文字一致
    QString hoverBg = grayTheme ? "#333333" : "#f5f5f5";  // 与导航栏悬停背景一致
    
    // 滚动条样式（与主题匹配）
    QString scrollBarStyle = grayTheme
        ? "QScrollBar:vertical { border: none; background: #2b2b2b; width: 12px; border-radius: 6px; }"
          "QScrollBar::handle:vertical { background: #555555; border-radius: 6px; min-height: 30px; margin: 2px; }"
          "QScrollBar::handle:vertical:hover { background: #666666; }"
          "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
          "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: #2b2b2b; }"
        : "QScrollBar:vertical { border: none; background: #f5f5f5; width: 12px; border-radius: 6px; }"
          "QScrollBar::handle:vertical { background: #c0c0c0; border-radius: 6px; min-height: 30px; margin: 2px; }"
          "QScrollBar::handle:vertical:hover { background: #a0a0a0; }"
          "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
          "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: #f5f5f5; }";
    
    QString listStyle = grayTheme
        ? QString("QListWidget { border: none; background: transparent; color: #e0e0e0; padding: 4px; }"
                  "QListWidget::item { padding: 8px 10px; border-bottom: 1px solid rgba(255, 255, 255, 0.1); border-radius: 4px; }"
                  "QListWidget::item:selected { background: %1; color: %2; }"
                  "QListWidget::item:hover { background: %3; }"
                  "QListWidget::item:selected:hover { background: %1; }")
          .arg(selectedBg).arg(selectedText).arg(hoverBg)
        : QString("QListWidget { border: none; background: transparent; color: #000000; padding: 4px; }"
                  "QListWidget::item { padding: 8px 10px; border-bottom: 1px solid #f0f0f0; border-radius: 4px; }"
                  "QListWidget::item:selected { background: %1; color: %2; }"
                  "QListWidget::item:hover { background: %3; }"
                  "QListWidget::item:selected:hover { background: %1; }")
          .arg(selectedBg).arg(selectedText).arg(hoverBg);
    
    // 按类型顺序创建选项卡
    for (const QString& type : typeOrder) {
        if (typeGroups.contains(type) && !typeGroups[type].isEmpty()) {
            QWidget* tab = new QWidget();
            QVBoxLayout* layout = new QVBoxLayout(tab);
            layout->setContentsMargins(0, 0, 0, 0);
            
            QListWidget* list = new QListWidget();
            list->setStyleSheet(listStyle);
            list->verticalScrollBar()->setStyleSheet(scrollBarStyle);
            layout->addWidget(list);
            
            m_promptCategoryTabs->addTab(tab, type);
            m_promptTypeLists[type] = list;
            
            for (const PromptTemplate& tmpl : typeGroups[type]) {
                QString displayName = QString("%1 [%2]").arg(tmpl.name, tmpl.category);
                QListWidgetItem* item = new QListWidgetItem(displayName);
                item->setData(Qt::UserRole, tmpl.content);
                item->setData(Qt::UserRole + 1, tmpl.name);
                list->addItem(item);
            }
            
            connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
                QString content = item->data(Qt::UserRole).toString();
                if (!content.isEmpty() && m_promptEdit) {
                    m_promptEdit->setPlainText(content);
                    qDebug() << "选择提示词模板:" << item->data(Qt::UserRole + 1).toString();
                }
            });
        }
    }
    
    // 处理其他未在typeOrder中的类型
    for (auto it = typeGroups.begin(); it != typeGroups.end(); ++it) {
        if (!typeOrder.contains(it.key()) && !it.value().isEmpty()) {
            QWidget* tab = new QWidget();
            QVBoxLayout* layout = new QVBoxLayout(tab);
            layout->setContentsMargins(0, 0, 0, 0);
            
            QListWidget* list = new QListWidget();
            list->setStyleSheet(listStyle);
            list->verticalScrollBar()->setStyleSheet(scrollBarStyle);
            layout->addWidget(list);
            
            m_promptCategoryTabs->addTab(tab, it.key());
            m_promptTypeLists[it.key()] = list;
            
            for (const PromptTemplate& tmpl : it.value()) {
                QString displayName = QString("%1 [%2]").arg(tmpl.name, tmpl.category);
                QListWidgetItem* item = new QListWidgetItem(displayName);
                item->setData(Qt::UserRole, tmpl.content);
                item->setData(Qt::UserRole + 1, tmpl.name);
                list->addItem(item);
            }
            
            connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
                QString content = item->data(Qt::UserRole).toString();
                if (!content.isEmpty() && m_promptEdit) {
                    m_promptEdit->setPlainText(content);
                    qDebug() << "选择提示词模板:" << item->data(Qt::UserRole + 1).toString();
                }
            });
        }
    }
    
    // 重新注册快捷键
    setupShortcuts();
}

void MainWindow::onSidebarToggleClicked()
{
    m_sidebarCollapsed = !m_sidebarCollapsed;
    
    if (m_sidebarCollapsed) {
        // 折叠：将侧边栏宽度设为较小，只显示按钮图标
        m_sidebarPanel->setMaximumWidth(60);
        m_sidebarPanel->setMinimumWidth(60);
        QIcon sidebarIcon(":/res/sidebar.png");
        QTransform transform;
        transform.rotate(180); // 旋转180度显示为右箭头
        QPixmap pixmap = sidebarIcon.pixmap(16, 16);
        QPixmap rotatedPixmap = pixmap.transformed(transform, Qt::SmoothTransformation);
        m_sidebarToggleBtn->setIcon(QIcon(rotatedPixmap));
        m_sidebarToggleBtn->setToolTip("展开侧边栏");
        
        // 隐藏配置区域、分隔线和主题切换按钮
        QLayout* layout = m_sidebarPanel->layout();
        if (layout) {
            for (int i = 0; i < layout->count(); ++i) {
                QLayoutItem* item = layout->itemAt(i);
                if (item && item->widget()) {
                    QWidget* widget = item->widget();
                    // 保留header（包含折叠按钮）和导航按钮
                    if (widget->objectName() != "sidebarHeader" && 
                        widget != m_sidebarToggleBtn &&
                        widget != m_navHomeBtn &&
                        widget != m_navHistoryBtn &&
                        widget != m_navSettingsBtn) {
                        widget->hide();
                    }
                }
            }
        }
        
        // 将"开始识别"按钮从配置区域移到侧边栏
        if (m_recognizeBtn && m_recognizeBtn->parent() != m_sidebarPanel) {
            QWidget* parentWidget = qobject_cast<QWidget*>(m_recognizeBtn->parent());
            if (parentWidget) {
                QLayout* parentLayout = parentWidget->layout();
                if (parentLayout) {
                    parentLayout->removeWidget(m_recognizeBtn);
                }
            }
            m_recognizeBtn->setParent(m_sidebarPanel);
            // 获取侧边栏布局并插入按钮
            QVBoxLayout* sidebarLayout = qobject_cast<QVBoxLayout*>(m_sidebarPanel->layout());
            if (sidebarLayout) {
                // 找到导航按钮之后的位置插入（header + 3个导航按钮 = 4个位置）
                int insertIndex = 4;
                sidebarLayout->insertWidget(insertIndex, m_recognizeBtn);
            }
        }
        
        // 导航按钮和识别按钮：只显示图标，隐藏文字，统一为正方形按钮
        QString collapsedBtnStyle = 
            "QPushButton { "
            "  text-align: center; "
            "  padding: 0px; "
            "  font-size: 10pt; "
            "  font-weight: normal; "
            "  color: #ffffff; "
            "  border: none; "
            "  background: transparent;"
            "}"
            "QPushButton:hover:enabled { "
            "  background: #3a3a3a; "
            "  color: #ffffff;"
            "}"
            "QPushButton:hover:disabled { "
            "  background: transparent;"
            "}"
            "QPushButton:checked { "
            "  background: #404040; "
            "  color: #ffffff; "
            "  font-weight: normal; "
            "  border-left: 3px solid #ffffff;"
            "}"
            "QPushButton:disabled { "
            "  color: #666;"
            "}"
            "QPushButton::icon { "
            "  margin-right: 0px;"
            "}";
        
        // 统一设置为固定大小的正方形按钮
        const int collapsedBtnSize = 48;
        m_navHomeBtn->setStyleSheet(collapsedBtnStyle);
        m_navHomeBtn->setText(""); // 隐藏文字
        m_navHomeBtn->setFixedSize(collapsedBtnSize, collapsedBtnSize);
        m_navHomeBtn->setMinimumHeight(collapsedBtnSize);
        m_navHomeBtn->setIconSize(QSize(24, 24));
        
        m_navHistoryBtn->setStyleSheet(collapsedBtnStyle);
        m_navHistoryBtn->setText(""); // 隐藏文字
        m_navHistoryBtn->setFixedSize(collapsedBtnSize, collapsedBtnSize);
        m_navHistoryBtn->setMinimumHeight(collapsedBtnSize);
        m_navHistoryBtn->setIconSize(QSize(24, 24));
        
        m_navSettingsBtn->setStyleSheet(collapsedBtnStyle);
        m_navSettingsBtn->setText(""); // 隐藏文字
        m_navSettingsBtn->setFixedSize(collapsedBtnSize, collapsedBtnSize);
        m_navSettingsBtn->setMinimumHeight(collapsedBtnSize);
        m_navSettingsBtn->setIconSize(QSize(24, 24));
        
        // "开始识别"按钮应用折叠样式
        if (m_recognizeBtn) {
            m_recognizeBtn->setStyleSheet(collapsedBtnStyle);
            m_recognizeBtn->setText(""); // 隐藏文字
            m_recognizeBtn->setFixedSize(collapsedBtnSize, collapsedBtnSize);
            m_recognizeBtn->setMinimumHeight(collapsedBtnSize);
            m_recognizeBtn->setIconSize(QSize(24, 24));
            // 更新 tooltip（包含快捷键）
            updateButtonTooltips();
            m_recognizeBtn->show(); // 确保显示
        }
    } else {
        // 展开：恢复侧边栏
        m_sidebarPanel->setMinimumWidth(300); // 恢复后：侧边栏最小宽度为300
        m_sidebarPanel->setMaximumWidth(QWIDGETSIZE_MAX); // 恢复后：侧边栏最大宽度为 无限制
        QIcon sidebarIcon(":/res/sidebar.png");
        m_sidebarToggleBtn->setIcon(sidebarIcon);
        m_sidebarToggleBtn->setToolTip("收起侧边栏");
        // 显示所有子控件
        QLayout* layout = m_sidebarPanel->layout();
        if (layout) {
            for (int i = 0; i < layout->count(); ++i) {
                QLayoutItem* item = layout->itemAt(i);
                if (item && item->widget()) {
                    item->widget()->show();
                }
            }
        }
        
        // 将"开始识别"按钮移回配置区域
        if (m_recognizeBtn) {
            // 获取侧边栏布局
            QVBoxLayout* sidebarLayout = qobject_cast<QVBoxLayout*>(m_sidebarPanel->layout());
            if (sidebarLayout) {
                // 从侧边栏布局中移除
                sidebarLayout->removeWidget(m_recognizeBtn);
                
                // 找到配置区域的布局并添加回去
                QWidget* configWidget = nullptr;
                for (int i = 0; i < sidebarLayout->count(); ++i) {
                    QLayoutItem* item = sidebarLayout->itemAt(i);
                    if (item && item->widget() && item->widget()->property("configWidget").toBool()) {
                        configWidget = item->widget();
                        break;
                    }
                }
                
                if (configWidget) {
                    QVBoxLayout* configLayout = qobject_cast<QVBoxLayout*>(configWidget->layout());
                if (configLayout) {
                    m_recognizeBtn->setParent(configWidget);
                    // 添加到提示词编辑框之后
                    configLayout->addWidget(m_recognizeBtn);
                    // 恢复按钮的原始文字和样式
                    m_recognizeBtn->setText(m_recognizeBtn->property("originalText").toString());
                    m_recognizeBtn->setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
                    m_recognizeBtn->setMinimumHeight(44);
                    m_recognizeBtn->setIconSize(QSize(18, 18));
                    // 更新 tooltip（包含快捷键）
                    updateButtonTooltips();
                }
                }
            }
        }
        
        // 恢复导航按钮的文字和图标大小
        m_navHomeBtn->setText(m_navHomeBtn->property("originalText").toString());
        m_navHomeBtn->setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX); // 取消固定大小
        m_navHomeBtn->setMinimumHeight(56);
        m_navHomeBtn->setIconSize(QSize(22, 22));
        
        m_navHistoryBtn->setText(m_navHistoryBtn->property("originalText").toString());
        m_navHistoryBtn->setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX); // 取消固定大小
        m_navHistoryBtn->setMinimumHeight(56);
        m_navHistoryBtn->setIconSize(QSize(22, 22));
        
        m_navSettingsBtn->setText(m_navSettingsBtn->property("originalText").toString());
        m_navSettingsBtn->setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX); // 取消固定大小
        m_navSettingsBtn->setMinimumHeight(56);
        m_navSettingsBtn->setIconSize(QSize(22, 22));
        
        // 重新应用当前主题样式，确保文字颜色正确
        applyTheme(m_isGrayTheme);
    }
    
    // 更新分割器大小
    if (m_mainSplitter) {
        if (m_sidebarCollapsed) {
            m_mainSplitter->setSizes({60, 1000});
        } else {
            m_mainSplitter->setSizes({220, 900});
        }
    }
}

void MainWindow::onThemeToggleClicked()
{
    applyTheme(!m_isGrayTheme);
    // 重新加载提示词，确保主题切换后样式与数据同步
    reloadPromptTabs();
}

void MainWindow::reloadPromptTabs()
{
    if (!m_configManager || !m_promptCategoryTabs) return;
    
    // 清空现有
    while (m_promptCategoryTabs->count() > 0) {
        m_promptCategoryTabs->removeTab(0);
    }
    m_promptTypeLists.clear();
    
    QList<PromptTemplate> templates = m_configManager->getPromptTemplates();
    
    // 按类型分组
    QMap<QString, QList<PromptTemplate>> typeGroups;
    for (const PromptTemplate& tmpl : templates) {
        typeGroups[tmpl.type].append(tmpl);
    }
    
    QStringList typeOrder = {"识别", "翻译", "解答", "整理"};
    
    bool grayTheme = m_isGrayTheme;
    
    QString scrollBarStyle = grayTheme
        ? "QScrollBar:vertical { border: none; background: #2b2b2b; width: 12px; border-radius: 6px; }"
          "QScrollBar::handle:vertical { background: #555555; border-radius: 6px; min-height: 30px; margin: 2px; }"
          "QScrollBar::handle:vertical:hover { background: #666666; }"
          "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
          "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: #2b2b2b; }"
        : "QScrollBar:vertical { border: none; background: #f5f5f5; width: 12px; border-radius: 6px; }"
          "QScrollBar::handle:vertical { background: #c0c0c0; border-radius: 6px; min-height: 30px; margin: 2px; }"
          "QScrollBar::handle:vertical:hover { background: #a0a0a0; }"
          "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
          "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: #f5f5f5; }";
    
    QString listStyle = grayTheme
        ? "QListWidget { "
          "  border: none; "
          "  background: #2b2b2b; "
          "  color: #e0e0e0; "
          "  padding: 4px; "
          "}"
          "QListWidget::item { "
          "  padding: 10px 12px; "
          "  border-bottom: 1px solid rgba(255, 255, 255, 0.08); "
          "  border-radius: 4px; "
          "}"
          "QListWidget::item:selected { "
          "  background: rgba(255, 255, 255, 0.12); "
          "  color: #ffffff; "
          "}"
          "QListWidget::item:hover { "
          "  background: rgba(255, 255, 255, 0.08); "
          "}"
          "QListWidget::item:selected:hover { "
          "  background: rgba(255, 255, 255, 0.12); "
          "}"
        : "QListWidget { "
          "  border: none; "
          "  background: transparent; "
          "  color: #000000; "
          "  padding: 4px; "
          "}"
          "QListWidget::item { "
          "  padding: 8px 10px; "
          "  border-bottom: 1px solid #f0f0f0; "
          "  border-radius: 4px; "
          "}"
          "QListWidget::item:selected { "
          "  background: #e0e0e0; "
          "  color: #000000; "
          "}"
          "QListWidget::item:hover { "
          "  background: #f5f5f5; "
          "}"
          "QListWidget::item:selected:hover { "
          "  background: #e0e0e0; "
          "}";
    
    auto addTabForType = [&](const QString& type, const QList<PromptTemplate>& listData) {
        QWidget* tab = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(tab);
        layout->setContentsMargins(0, 0, 0, 0);
        
        QListWidget* list = new QListWidget();
        list->setStyleSheet(listStyle);
        list->verticalScrollBar()->setStyleSheet(scrollBarStyle);
        layout->addWidget(list);
        
        m_promptCategoryTabs->addTab(tab, type);
        m_promptTypeLists[type] = list;
        
        for (const PromptTemplate& tmpl : listData) {
            QString displayName = QString("%1 [%2]").arg(tmpl.name, tmpl.category);
            QListWidgetItem* item = new QListWidgetItem(displayName);
            item->setData(Qt::UserRole, tmpl.content);
            item->setData(Qt::UserRole + 1, tmpl.name);
            item->setData(Qt::UserRole + 2, tmpl.name);
            list->addItem(item);
        }
        
        connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
            QString content = item->data(Qt::UserRole).toString();
            if (!content.isEmpty() && m_promptEdit) {
                m_promptEdit->setPlainText(content);
                qDebug() << "选择提示词模板:" << item->data(Qt::UserRole + 1).toString();
            }
        });
    };
    
    for (const QString& type : typeOrder) {
        if (typeGroups.contains(type) && !typeGroups[type].isEmpty()) {
            addTabForType(type, typeGroups[type]);
        }
    }
    for (auto it = typeGroups.begin(); it != typeGroups.end(); ++it) {
        if (!typeOrder.contains(it.key()) && !it.value().isEmpty()) {
            addTabForType(it.key(), it.value());
        }
    }
    
    // 默认选中
    if (!m_promptTypeLists.isEmpty()) {
        m_promptCategoryTabs->setCurrentIndex(0);
        QListWidget* firstList = m_promptTypeLists.begin().value();
        if (firstList && firstList->count() > 0) {
            firstList->setCurrentRow(0);
            QString content = firstList->item(0)->data(Qt::UserRole).toString();
            if (!content.isEmpty() && m_promptEdit) {
                m_promptEdit->setPlainText(content);
                qDebug() << "选择提示词模板:" << firstList->item(0)->data(Qt::UserRole + 1).toString();
            }
        }
    }
}

void MainWindow::applyTheme(bool grayTheme)
{
    m_isGrayTheme = grayTheme;
    
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
    
    if (m_themeToggleBtn)
    {
        // 按钮使用图标，不再显示文本
        // 单独设置主题切换按钮样式（不受侧边栏通用样式影响）
        if (grayTheme) {
            // 灰主题：深色按钮
            m_themeToggleBtn->setStyleSheet(
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
            m_themeToggleBtn->setStyleSheet(
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
    
    if (m_contentPanel)
    {
        m_contentPanel->setStyleSheet(QString("QWidget { background-color: %1; }").arg(bg));
    }
    
    if (m_centralWidget)
    {
        m_centralWidget->setStyleSheet(QString("QWidget { background-color: %1; }").arg(bg));
    }
    
    if (m_sidebarPanel)
    {
        // 设置侧边栏背景
        m_sidebarPanel->setStyleSheet(QString("QWidget { background: %1; }").arg(sidebarBg));
        
        // 折叠状态下保持正方形图标按钮样式，避免 applyTheme 覆盖后图标错位
        if (m_sidebarCollapsed)
        {
            const int collapsedBtnSize = 48;
            QString collapsedBtnStyle = 
                "QPushButton { "
                "  text-align: center; "
                "  padding: 0px; "
                "  font-size: 10pt; "
                "  font-weight: normal; "
                "  color: #ffffff; "
                "  border: none; "
                "  background: transparent;"
                "}"
                "QPushButton:hover:enabled { "
                "  background: #3a3a3a; "
                "  color: #ffffff;"
                "}"
                "QPushButton:hover:disabled { "
                "  background: transparent;"
                "}"
                "QPushButton:checked { "
                "  background: #404040; "
                "  color: #ffffff; "
                "  font-weight: normal; "
                "  border-left: 3px solid #ffffff;"
                "}"
                "QPushButton:disabled { "
                "  color: #666;"
                "}"
                "QPushButton::icon { "
                "  margin-right: 0px;"
                "}";
            
            auto applyCollapsedStyle = [&](QPushButton* btn) {
                if (!btn) return;
                btn->setStyleSheet(collapsedBtnStyle);
                btn->setText("");
                btn->setFixedSize(collapsedBtnSize, collapsedBtnSize);
                btn->setMinimumHeight(collapsedBtnSize);
                btn->setIconSize(QSize(24, 24));
            };
            
            applyCollapsedStyle(m_navHomeBtn);
            applyCollapsedStyle(m_navHistoryBtn);
            applyCollapsedStyle(m_navSettingsBtn);
            applyCollapsedStyle(m_recognizeBtn);
        }
        else
        {
            // 确保文字颜色有足够对比度 - 直接为每个按钮设置样式
            QString normalTextColor = grayTheme ? "#ffffff" : "#000000";  // 正常状态：深色主题用纯白，白色主题用纯黑
            QString checkedTextColor = grayTheme ? "#ffffff" : "#000000"; // 选中状态：深色主题用纯白，白色主题用纯黑
            QString hoverTextColor = grayTheme ? "#ffffff" : "#000000";   // 悬停状态：深色主题用纯白，白色主题用纯黑
            
            QString navButtonStyle = QString(
                "QPushButton { "
                "  text-align: left; "
                "  padding: 14px 20px; "
                "  font-size: 11pt; "
                "  font-weight: normal; "
                "  color: %1; "
                "  border: none; "
                "  background: transparent;"
                "  border-radius: 0px;"
                "}"
                "QPushButton:hover { "
                "  background: %2; "
                "  color: %3;"
                "}"
                "QPushButton:checked { "
                "  background: %4; "
                "  color: %5; "
                "  font-weight: normal; "
                "  border-left: 4px solid %6;"
                "}"
                "QPushButton::icon { "
                "  margin-right: 12px;"
                "}"
            )
                .arg(normalTextColor)
                .arg(navHover)
                .arg(hoverTextColor)
                .arg(navChecked)
                .arg(checkedTextColor)
                .arg(navCheckedBorder);
            
            // 直接为每个导航按钮设置样式
            if (m_navHomeBtn) m_navHomeBtn->setStyleSheet(navButtonStyle);
            if (m_navHistoryBtn) m_navHistoryBtn->setStyleSheet(navButtonStyle);
            if (m_navSettingsBtn) m_navSettingsBtn->setStyleSheet(navButtonStyle);
        }
    }
    
    // 卡片统一样式（在 createCard 中打了 themeCard 标记）
    auto cards = findChildren<QWidget*>();
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
    if (m_uploadBtn && m_pasteBtn)
    {
        QString mainBtnStyle = grayTheme
            ? "QPushButton { "
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
              "}"
            : "QPushButton { "
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
        m_uploadBtn->setStyleSheet(mainBtnStyle);
        m_pasteBtn->setStyleSheet(mainBtnStyle);
    }
    
    if (m_recognizeBtn)
    {
        // 融入导航栏风格的按钮样式
        QString recognizeBtnStyle = grayTheme
            ? "QPushButton { "
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
              "}"
            : "QPushButton { "
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
        m_recognizeBtn->setStyleSheet(recognizeBtnStyle);
    }
    
    if (m_copyResultBtn && m_exportBtn)
    {
        QString resultBtnStyle = grayTheme
            ? "QPushButton { "
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
              "}"
            : "QPushButton { "
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
        m_copyResultBtn->setStyleSheet(resultBtnStyle);
        m_exportBtn->setStyleSheet(resultBtnStyle);
        if (m_previewResultBtn)
            m_previewResultBtn->setStyleSheet(resultBtnStyle);
    }
    
    // 其他按钮（深色风格）
    auto buttons = findChildren<QPushButton*>();
    for (QPushButton* btn : buttons)
    {
        if (btn == m_navHomeBtn || btn == m_navHistoryBtn || btn == m_navSettingsBtn)
            continue; // 已在侧边栏样式中处理
        if (btn == m_themeToggleBtn)
            continue; // 切换按钮自身文字已处理
        if (btn == m_uploadBtn || btn == m_pasteBtn || btn == m_recognizeBtn || 
            btn == m_copyResultBtn || btn == m_exportBtn || btn == m_previewResultBtn)
            continue; // 主页面按钮已单独处理
        
        btn->setStyleSheet(
            QString(
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
                .arg(btnDarkHover));
    }
    
    // 标签统一文字颜色（排除配置区域的标签，它们有专门样式）
    auto labels = findChildren<QLabel*>();
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
            QString hintColor = grayTheme ? "rgba(200, 200, 200, 0.85)" : "rgba(100, 100, 100, 0.9)";
            lbl->setStyleSheet(QString("QLabel { color: %1; background: transparent; border: none; font-size: 10pt; padding: 3px; margin: 0px; }").arg(hintColor));
            continue;
        }
        lbl->setStyleSheet(QString("QLabel { color: %1; background: transparent; }").arg(textColor));
    }
    
    // 更新侧边栏配置组件样式
    if (m_modelComboBox)
    {
        QString comboStyle = grayTheme 
            ? "QComboBox { padding: 10px 14px; font-size: 10pt; font-weight: normal; border: 1px solid rgba(255, 255, 255, 0.2); border-radius: 8px; background: rgba(0, 0, 0, 0.2); color: #e0e0e0; }"
               "QComboBox:hover { background: rgba(0, 0, 0, 0.3); border-color: rgba(255, 255, 255, 0.3); color: #ffffff; }"
               "QComboBox::drop-down { border: none; width: 28px; }"
               "QComboBox::down-arrow { image: none; border-left: 4px solid transparent; border-right: 4px solid transparent; border-top: 5px solid #b0b0b0; width: 0; height: 0; }"
               "QComboBox QAbstractItemView { background: #2c2c2c; color: #e0e0e0; border: 1px solid rgba(255, 255, 255, 0.2); border-radius: 6px; padding: 4px; selection-background-color: #404040; selection-color: #ffffff; }"
            : "QComboBox { padding: 10px 14px; font-size: 10pt; font-weight: normal; border: 1px solid #e0e0e0; border-radius: 8px; background: #ffffff; color: #000000; }"
               "QComboBox:hover { background: #f5f5f5; border-color: #ccc; color: #000000; }"
               "QComboBox::drop-down { border: none; width: 28px; }"
               "QComboBox::down-arrow { image: none; border-left: 4px solid transparent; border-right: 4px solid transparent; border-top: 5px solid #666666; width: 0; height: 0; }"
               "QComboBox QAbstractItemView { background: #ffffff; color: #000000; border: 1px solid #e0e0e0; border-radius: 6px; padding: 4px; selection-background-color: #f5f5f5; selection-color: #000000; }";
        m_modelComboBox->setStyleSheet(comboStyle);
    }
    
    // 更新提示词选项卡样式
    if (m_promptCategoryTabs)
    {
        QString tabStyle = grayTheme
            ? "QTabWidget::pane { "
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
              "}"
            : "QTabWidget::pane { "
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
        m_promptCategoryTabs->setStyleSheet(tabStyle);
        
        // 更新列表样式（与导航栏选中样式一致）
        QString selectedBg = navChecked;  // 使用导航栏的选中背景色
        QString selectedBorder = navCheckedBorder;  // 使用导航栏的选中边框色
        QString selectedText = grayTheme ? "#ffffff" : "#000000";  // 与导航栏选中文字一致
        QString hoverBg = navHover;  // 使用导航栏的悬停背景色
        
        // 滚动条样式（与主题匹配）
        QString scrollBarStyle = grayTheme
            ? "QScrollBar:vertical { border: none; background: #2b2b2b; width: 12px; border-radius: 6px; }"
              "QScrollBar::handle:vertical { background: #555555; border-radius: 6px; min-height: 30px; margin: 2px; }"
              "QScrollBar::handle:vertical:hover { background: #666666; }"
              "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
              "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: #2b2b2b; }"
            : "QScrollBar:vertical { border: none; background: #f5f5f5; width: 12px; border-radius: 6px; }"
              "QScrollBar::handle:vertical { background: #c0c0c0; border-radius: 6px; min-height: 30px; margin: 2px; }"
              "QScrollBar::handle:vertical:hover { background: #a0a0a0; }"
              "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
              "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: #f5f5f5; }";
        
        QString listStyle = grayTheme
            ? QString("QListWidget { border: none; background: transparent; color: #e0e0e0; padding: 4px; }"
                      "QListWidget::item { padding: 8px 10px; border-bottom: 1px solid rgba(255, 255, 255, 0.1); border-radius: 4px; }"
                      "QListWidget::item:selected { background: %1; color: %2; }"
                      "QListWidget::item:hover { background: %3; }"
                      "QListWidget::item:selected:hover { background: %1; }")
              .arg(selectedBg).arg(selectedText).arg(hoverBg)
            : QString("QListWidget { border: none; background: transparent; color: #000000; padding: 4px; }"
                      "QListWidget::item { padding: 8px 10px; border-bottom: 1px solid #f0f0f0; border-radius: 4px; }"
                      "QListWidget::item:selected { background: %1; color: %2; }"
                      "QListWidget::item:hover { background: %3; }"
                      "QListWidget::item:selected:hover { background: %1; }")
              .arg(selectedBg).arg(selectedText).arg(hoverBg);
        
        // 为所有列表设置滚动条样式
        for (QListWidget* list : m_promptTypeLists.values()) {
            if (list) {
                list->verticalScrollBar()->setStyleSheet(scrollBarStyle);
            }
        }
        
        for (QListWidget* list : m_promptTypeLists.values()) {
            if (list) {
                list->setStyleSheet(listStyle);
            }
        }
    }
    
    if (m_promptEdit)
    {
        QString editStyle = grayTheme 
            ? "QTextEdit { padding: 12px; font-size: 10pt; font-weight: 400; border: 1px solid rgba(255, 255, 255, 0.2); border-radius: 8px; background: rgba(0, 0, 0, 0.25); color: #e0e0e0; line-height: 1.5; }"
               "QTextEdit:focus { border-color: rgba(255, 255, 255, 0.35); background: rgba(0, 0, 0, 0.35); }"
            : "QTextEdit { padding: 12px; font-size: 10pt; font-weight: 400; border: 1px solid #e0e0e0; border-radius: 8px; background: #ffffff; color: #000000; line-height: 1.5; }"
               "QTextEdit:focus { border-color: #000000; background: #ffffff; }";
        m_promptEdit->setStyleSheet(editStyle);
    }

    // 结果文本区域样式
    if (m_resultText)
    {
        QString resultStyle = grayTheme
            ? "QTextEdit { padding: 15px; font-size: 11pt; border: 1px solid #3a3a3a; border-radius: 10px; background: #1f1f1f; color: #e0e0e0; line-height: 1.6; }"
            : "QTextEdit { padding: 15px; font-size: 11pt; border: 2px solid #ddd; border-radius: 8px; background: white; color: #333; line-height: 1.6; }";
        m_resultText->setStyleSheet(resultStyle);
    }
    
    // 更新配置区域的标签样式（"模型"、"提示词"）
    auto allWidgets = m_sidebarPanel->findChildren<QWidget*>();
    for (QWidget* widget : allWidgets)
    {
        if (widget->property("configWidget").toBool())
        {
            auto labels = widget->findChildren<QLabel*>();
            for (QLabel* lbl : labels)
            {
                QString labelStyle = grayTheme
                    ? "QLabel { color: #b0b0b0; font-size: 10pt; font-weight: normal; background: transparent; margin-bottom: 6px; }"
                    : "QLabel { color: #666666; font-size: 10pt; font-weight: normal; background: transparent; margin-bottom: 6px; }";
                lbl->setStyleSheet(labelStyle);
            }
            break;
        }
    }
    
    // 更新分隔线样式
    auto separators = findChildren<QFrame*>();
    for (QFrame* sep : separators)
    {
        if (sep->frameShape() == QFrame::HLine)
        {
            QString sepColor = grayTheme ? "rgba(255, 255, 255, 0.1)" : "rgba(0, 0, 0, 0.1)";
            sep->setStyleSheet(QString("QFrame { background: %1; max-height: 1px; }").arg(sepColor));
        }
    }
    
    // 更新折叠按钮样式
    if (m_sidebarToggleBtn)
    {
        QString toggleBtnColor = grayTheme ? "#b0b0b0" : "#666666";
        QString toggleBtnHoverBg = grayTheme ? "rgba(255, 255, 255, 0.1)" : "rgba(0, 0, 0, 0.05)";
        QString toggleBtnHoverColor = grayTheme ? "#ffffff" : "#000000";
        m_sidebarToggleBtn->setStyleSheet(
            QString(
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
                .arg(toggleBtnColor)
                .arg(toggleBtnHoverBg)
                .arg(toggleBtnHoverColor));
    }
    
    // 更新 QSplitter 样式
    auto splitters = findChildren<QSplitter*>();
    for (QSplitter* splitter : splitters)
    {
        QString handleColor = grayTheme ? "#3a3a3a" : "#ededed";
        QString handleBorder = grayTheme ? "#2a2a2a" : "#dcdcdc";
        QString handleHoverColor = grayTheme ? "#505050" : "#d8d8d8";
        QString handleHoverBorder = grayTheme ? "#3c3c3c" : "#c2c2c2";
        splitter->setStyleSheet(
            QString(
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
                .arg(handleColor)
                .arg(handleBorder)
                .arg(handleHoverColor)
                .arg(handleHoverBorder));
        splitter->setHandleWidth(6);
        if (QSplitterHandle* handle = splitter->handle(1)) {
            handle->setCursor(Qt::SplitHCursor);
        }
    }
    
    // 更新图片显示区域样式
    if (m_imageLabel)
    {
        QString imageLabelStyle = grayTheme
            ? "QLabel { background: #3a3a3a; border: 3px dashed rgba(255, 255, 255, 0.2); border-radius: 12px; color: #999; font-size: 12pt; }"
            : "QLabel { background: white; border: 3px dashed #ddd; border-radius: 12px; color: #999; font-size: 12pt; }";
        m_imageLabel->setStyleSheet(imageLabelStyle);
    }
    
    // 更新结果文本区域样式
    if (m_resultText)
    {
        QString resultTextStyle = grayTheme
            ? "QTextEdit { padding: 15px; font-size: 11pt; border: 2px solid rgba(255, 255, 255, 0.2); border-radius: 8px; background: #3a3a3a; color: #e0e0e0; line-height: 1.6; }"
            : "QTextEdit { padding: 15px; font-size: 11pt; border: 2px solid #ddd; border-radius: 8px; background: white; color: #333; line-height: 1.6; }";
        m_resultText->setStyleSheet(resultTextStyle);
    }
    
    // 更新历史记录列表样式
    if (m_historyList)
    {
        QString historyListStyle = grayTheme
            ? "QListWidget { background: #3a3a3a; border: 2px solid rgba(255, 255, 255, 0.2); border-radius: 12px; padding: 10px; font-size: 11pt; color: #e0e0e0; }"
              "QListWidget::item { padding: 15px; border-bottom: 1px solid rgba(255, 255, 255, 0.1); color: #e0e0e0; }"
              "QListWidget::item:hover { background: #404040; }"
              "QListWidget::item:selected { background: #505050; color: #ffffff; }"
            : "QListWidget { background: white; border: 2px solid #ddd; border-radius: 12px; padding: 10px; font-size: 11pt; color: #333; }"
              "QListWidget::item { padding: 15px; border-bottom: 1px solid #eee; color: #333; }"
              "QListWidget::item:hover { background: #f5f5f5; }"
              "QListWidget::item:selected { background: #e0e0e0; color: #000000; }";
        m_historyList->setStyleSheet(historyListStyle);
    }
    
    // 更新状态栏样式
    QString statusBarStyle = grayTheme
        ? "QStatusBar { background: #2c2c2c; border-top: 1px solid rgba(255, 255, 255, 0.1); color: #e0e0e0; }"
          "QStatusBar QLabel { color: #e0e0e0; }"
        : "QStatusBar { background: white; border-top: 1px solid #ddd; color: #666; }"
          "QStatusBar QLabel { color: #666; }";
    statusBar()->setStyleSheet(statusBarStyle);
    
    // 更新滚动区域背景
    auto scrollAreas = findChildren<QScrollArea*>();
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
    if (m_homePage)
    {
        QString pageBg = grayTheme ? bg : "#f5f5f5";
        m_homePage->setStyleSheet(QString("QWidget { background-color: %1; }").arg(pageBg));
    }
    if (m_historyPage)
    {
        QString pageBg = grayTheme ? bg : "#f5f5f5";
        m_historyPage->setStyleSheet(QString("QWidget { background-color: %1; }").arg(pageBg));
    }
    
    // 更新滚动条样式（增大尺寸以提高灵敏度）
    QString scrollBarStyle = grayTheme
        ? "QScrollBar:vertical { border: none; background: #2b2b2b; width: 18px; border-radius: 9px; }"
          "QScrollBar::handle:vertical { background: #555555; border-radius: 9px; min-height: 50px; margin: 2px; }"
          "QScrollBar::handle:vertical:hover { background: #666666; }"
          "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
          "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: #2b2b2b; }"
          "QScrollBar:horizontal { border: none; background: #2b2b2b; height: 18px; border-radius: 9px; }"
          "QScrollBar::handle:horizontal { background: #555555; border-radius: 9px; min-width: 50px; margin: 2px; }"
          "QScrollBar::handle:horizontal:hover { background: #666666; }"
          "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }"
          "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: #2b2b2b; }"
        : "QScrollBar:vertical { border: none; background: #f5f5f5; width: 18px; border-radius: 9px; }"
          "QScrollBar::handle:vertical { background: #c0c0c0; border-radius: 9px; min-height: 50px; margin: 2px; }"
          "QScrollBar::handle:vertical:hover { background: #a0a0a0; }"
          "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
          "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: #f5f5f5; }"
          "QScrollBar:horizontal { border: none; background: #f5f5f5; height: 18px; border-radius: 9px; }"
          "QScrollBar::handle:horizontal { background: #c0c0c0; border-radius: 9px; min-width: 50px; margin: 2px; }"
          "QScrollBar::handle:horizontal:hover { background: #a0a0a0; }"
          "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }"
          "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: #f5f5f5; }";
    setStyleSheet(scrollBarStyle);
    
    // 更新所有滚动区域的滚动步长
    auto scrollAreasForScroll = findChildren<QScrollArea*>();
    for (QScrollArea* area : scrollAreasForScroll)
    {
        area->verticalScrollBar()->setSingleStep(20);
        area->verticalScrollBar()->setPageStep(100);
        area->horizontalScrollBar()->setSingleStep(20);
        area->horizontalScrollBar()->setPageStep(100);
    }
    
    // 更新所有 QListWidget 和 QTextEdit 的滚动步长
    auto listWidgets = findChildren<QListWidget*>();
    for (QListWidget* list : listWidgets)
    {
        list->verticalScrollBar()->setSingleStep(20);
        list->verticalScrollBar()->setPageStep(100);
        list->horizontalScrollBar()->setSingleStep(20);
        list->horizontalScrollBar()->setPageStep(100);
    }
    
    auto textEdits = findChildren<QTextEdit*>();
    for (QTextEdit* edit : textEdits)
    {
        edit->verticalScrollBar()->setSingleStep(20);
        edit->verticalScrollBar()->setPageStep(100);
        edit->horizontalScrollBar()->setSingleStep(20);
        edit->horizontalScrollBar()->setPageStep(100);
    }
    
    // 更新配置区域标签颜色（通过查找配置区域的子标签）
    auto allWidgetsForConfig = m_sidebarPanel->findChildren<QWidget*>();
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
    
    // 更新首页和历史页面的背景色
    if (m_homePage)
    {
        m_homePage->setStyleSheet(QString("QWidget { background-color: %1; }").arg(bg));
    }
    
    if (m_historyPage)
    {
        m_historyPage->setStyleSheet(QString("QWidget { background-color: %1; }").arg(bg));
    }
    
    // 更新滚动区域背景色
    auto scrollAreasForBg = findChildren<QScrollArea*>();
    for (QScrollArea* area : scrollAreasForBg)
    {
        area->setStyleSheet(QString("QScrollArea { background-color: %1; border: none; }").arg(bg));
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasImage() || event->mimeData()->hasUrls())
    {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();

    if (mimeData->hasImage())
    {
        QImage image = qvariant_cast<QImage>(mimeData->imageData());
        loadImage(image, SubmitSource::DragDrop);
        event->acceptProposedAction();
        return;
    }

    if (mimeData->hasUrls())
    {
        QList<QUrl> urls = mimeData->urls();
        if (!urls.isEmpty())
        {
            QString filePath = urls.first().toLocalFile();
            QImage image(filePath);
            if (!image.isNull())
            {
                loadImage(image, SubmitSource::DragDrop);
                event->acceptProposedAction();
            }
        }
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // Ctrl+V 粘贴图片
    if (event->matches(QKeySequence::Paste))
    {
        onPasteImageClicked();
        return;
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // 若托盘可用，则隐藏窗口而非退出
    if (m_trayIcon && m_trayIcon->isVisible())
    {
        event->ignore();
        hide();
        showStatusMessage("程序已最小化到托盘，双击托盘图标可恢复");

        // 仅首次提示托盘气泡，避免频繁打扰
        if (!m_trayNotified)
        {
            m_trayIcon->showMessage("XS-VLM-OCR",
                                    "程序已隐藏到托盘，双击图标恢复窗口",
                                    QSystemTrayIcon::Information,
                                    2000);
            m_trayNotified = true;
        }
        return;
    }

    QMainWindow::closeEvent(event);
}

// 区域选择窗口类（用于截图）
class ScreenshotSelector : public QWidget {
public:
    explicit ScreenshotSelector(std::function<void(const QImage&)> onAccepted, 
                                std::function<void()> onRejected,
                                QWidget* parent = nullptr) 
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
    
protected:
    void paintEvent(QPaintEvent* event) override {
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
    
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            m_startPoint = event->pos();
            m_selectionRect = QRect();
            m_isSelecting = true;
            update();
        }
    }
    
    void mouseMoveEvent(QMouseEvent* event) override {
        if (m_isSelecting) {
            QPoint endPoint = event->pos();
            m_selectionRect = QRect(m_startPoint, endPoint).normalized();
            update();
        }
    }
    
    void mouseReleaseEvent(QMouseEvent* event) override {
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
    
    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Escape) {
            close();
            if (m_onRejected) {
                m_onRejected();
            }
        } else {
            QWidget::keyPressEvent(event);
        }
    }
    
private:
    QPoint m_startPoint;
    QRect m_selectionRect;
    QPixmap m_fullScreenshot;
    bool m_isSelecting;
    std::function<void(const QImage&)> m_onAccepted;
    std::function<void()> m_onRejected;
};

void MainWindow::onScreenshotShortcut()
{
    qDebug() << "截图快捷键触发";
    
    // 隐藏主窗口
    hide();
    QApplication::processEvents();
    
    // 等待一小段时间，确保窗口已隐藏
    QTimer::singleShot(200, this, [this]() {
        // 创建区域选择窗口
        ScreenshotSelector* selector = new ScreenshotSelector(
            // 接受回调
            [this](const QImage& selectedImage) {
                if (selectedImage.isNull()) {
                    QMessageBox::warning(this, "错误", "截图失败");
                    show();
                    return;
                }
                
                // 显示窗口并加载截图
                show();
                activateWindow();
                raise();
                
                loadImage(selectedImage, SubmitSource::Shortcut);
                
                qDebug() << "截图完成，尺寸:" << selectedImage.size();
                
                // 检查是否启用自动识别
                if (m_configManager && m_configManager->getSetting("auto_recognize_after_screenshot", false).toBool()) {
                    // 等待一小段时间确保图片已加载，然后自动识别
                    QTimer::singleShot(100, this, [this]() {
                        if (!m_currentImage.isNull() && m_recognizeBtn->isEnabled()) {
                            qDebug() << "自动开始识别截图";
                            onRecognizeClicked();
                        }
                    });
                }
            },
            // 拒绝回调
            [this]() {
                // 取消截图，显示窗口
                show();
                activateWindow();
                raise();
            }
        );
        
        // 窗口关闭时自动删除
        selector->setAttribute(Qt::WA_DeleteOnClose);
    });
}

void MainWindow::onRecognizeShortcut()
{
    qDebug() << "识别快捷键触发";
    
    // 允许在没有图片时也触发（支持纯文本AI询问）
    if (m_recognizeBtn->isEnabled()) {
        onRecognizeClicked();
    } else {
        showStatusMessage("模型未就绪，请检查模型配置", 2000);
    }
}

#ifdef _WIN32
bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, long* result)
{
    Q_UNUSED(eventType);
    Q_UNUSED(result);
    
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_HOTKEY) {
        int id = static_cast<int>(msg->wParam);
        
        if (id == m_screenshotHotKeyId) {
            qDebug() << "Windows全局热键触发：截图";
            QTimer::singleShot(0, this, &MainWindow::onScreenshotShortcut);
            return true;
        } else if (id == m_recognizeHotKeyId) {
            qDebug() << "Windows全局热键触发：识别";
            QTimer::singleShot(0, this, &MainWindow::onRecognizeShortcut);
            return true;
        }
    }
    
    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif
