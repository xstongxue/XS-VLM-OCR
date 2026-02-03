#include "MainWindow.h"
#include "SettingsDialog.h"
#include "ScreenshotSelector.h"
#include "../adapters/TesseractAdapter.h"
#include "../adapters/QwenAdapter.h"
#include "../adapters/CustomAdapter.h"
#include "../adapters/GLMAdapter.h"
#include "../adapters/PaddleAdapter.h"
#include "../adapters/DoubaoAdapter.h"
#include "../adapters/GeneralAdapter.h"
#include "../adapters/GeminiAdapter.h"
#include "../utils/ConfigManager.h"
#include "../utils/ThemeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QPrinter>
#include <QTextDocument>
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
#include <QAbstractScrollArea>
#include <QFileInfo>
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
      m_trayNotified(false), m_settingsChanging(false)
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
    delete m_historyManager;
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
    m_sidebarPanel = new SidebarWidget(this);
    connect(m_sidebarPanel, &SidebarWidget::collapseChanged, this, &MainWindow::onSidebarCollapseChanged);
    
    // 获取控件引用
    m_sidebarToggleBtn = m_sidebarPanel->sidebarToggleBtn();
    m_themeToggleBtn = m_sidebarPanel->themeToggleBtn();
    m_navHomeBtn = m_sidebarPanel->navHomeBtn();
    m_navHistoryBtn = m_sidebarPanel->navHistoryBtn();
    m_navSettingsBtn = m_sidebarPanel->navSettingsBtn();
    m_modelComboBox = m_sidebarPanel->modelComboBox();
    m_promptCategoryTabs = m_sidebarPanel->promptTabs();
    m_promptEdit = m_sidebarPanel->promptEdit();
    m_recognizeBtn = m_sidebarPanel->recognizeBtn();

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
    m_mainSplitter->setStretchFactor(0, 0);    // 侧边栏不伸缩
    m_mainSplitter->setCollapsible(0, false);  // 侧边栏不可折叠（通过按钮控制）
    m_mainSplitter->setStretchFactor(1, 1);    // 内容区伸缩因子为1
    m_mainSplitter->setSizes({300, 900});      // 设置初始大小，侧边栏宽度300，内容区宽度900
    m_mainSplitter->setChildrenCollapsible(false); // 设置子控件不可折叠
    m_mainSplitter->setOpaqueResize(true); // 设置分割线可拖拽
    m_mainSplitter->setHandleWidth(6);     // 设置分割线宽度
    if (QSplitterHandle* handle = m_mainSplitter->handle(1)) {
        handle->setCursor(Qt::SplitHCursor);    // 设置分割线鼠标样式
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
    homeScrollArea->verticalScrollBar()->setSingleStep(20);   // 每次滚动20像素
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
    
    // 图片显示容器（包含图片和关闭/切换按钮）
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
    m_closeImageBtn->hide();  // 初始隐藏，有图片时才显示
    m_closeImageBtn->raise(); // 确保按钮在最上层
    
    // 使用相对定位布局
    imageContainerLayout->addWidget(m_imageLabel);
    m_imageContainer->setLayout(imageContainerLayout);
    
    // 将关闭按钮添加到容器（使用绝对定位）
    m_closeImageBtn->setParent(m_imageContainer);

    // 批量切换按钮（左右箭头）
    QString batchNavStyle =
        "QPushButton { "
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

    m_prevImageBtn = new QPushButton("<");
    m_prevImageBtn->setFixedSize(30, 60);
    m_prevImageBtn->setStyleSheet(batchNavStyle);
    m_prevImageBtn->setCursor(Qt::PointingHandCursor);
    m_prevImageBtn->hide();
    m_prevImageBtn->setParent(m_imageContainer);

    m_nextImageBtn = new QPushButton(">");
    m_nextImageBtn->setFixedSize(30, 60);
    m_nextImageBtn->setStyleSheet(batchNavStyle);
    m_nextImageBtn->setCursor(Qt::PointingHandCursor);
    m_nextImageBtn->hide();
    m_nextImageBtn->setParent(m_imageContainer);

    // 批量信息标签
    m_batchInfoLabel = new QLabel(m_imageContainer);
    m_batchInfoLabel->setStyleSheet("QLabel { background: rgba(0,0,0,0.45); color: white; padding: 4px 8px; border-radius: 8px; font-size: 9pt; }");
    m_batchInfoLabel->hide();
    
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
    m_resultText->setMinimumHeight(180); // 保留基本展示高度
    m_resultText->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_resultText->setLineWrapMode(QTextEdit::WidgetWidth); // 去除滚动条后保持内容换行
    m_resultText->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_resultText->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_resultText->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
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
    
    // 筛选区域
    QHBoxLayout *filterLayout = new QHBoxLayout();
    filterLayout->setSpacing(10);
    
    m_startDateEdit = new QDateEdit(QDate::currentDate().addDays(-7));
    m_startDateEdit->setCalendarPopup(true);
    m_startDateEdit->setDisplayFormat("yyyy-MM-dd");
    m_startDateEdit->setFixedWidth(120);
    
    m_endDateEdit = new QDateEdit(QDate::currentDate());
    m_endDateEdit->setCalendarPopup(true);
    m_endDateEdit->setDisplayFormat("yyyy-MM-dd");
    m_endDateEdit->setFixedWidth(120);
    
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText("搜索内容或模型...");
    
    m_searchBtn = new QPushButton("查询");
    m_searchBtn->setFixedWidth(80);
    
    filterLayout->addWidget(new QLabel("从:"));
    filterLayout->addWidget(m_startDateEdit);
    filterLayout->addWidget(new QLabel("至:"));
    filterLayout->addWidget(m_endDateEdit);
    filterLayout->addWidget(m_searchEdit);
    filterLayout->addWidget(m_searchBtn);
    
    historyPageLayout->addLayout(filterLayout);

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
    
    // 分页控件
    QHBoxLayout *paginationLayout = new QHBoxLayout();
    m_prevPageBtn = new QPushButton("上一页");
    m_nextPageBtn = new QPushButton("下一页");
    m_pageLabel = new QLabel("第 1 页");
    m_pageLabel->setAlignment(Qt::AlignCenter);
    
    paginationLayout->addStretch();
    paginationLayout->addWidget(m_prevPageBtn);
    paginationLayout->addWidget(m_pageLabel);
    paginationLayout->addWidget(m_nextPageBtn);
    paginationLayout->addStretch();
    
    historyPageLayout->addLayout(paginationLayout);

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
    
    // UI 按钮连接
    connect(m_uploadBtn, &QPushButton::clicked, this, &MainWindow::onUploadImageClicked);
    connect(m_pasteBtn, &QPushButton::clicked, this, &MainWindow::onPasteImageClicked);
    connect(m_recognizeBtn, &QPushButton::clicked, this, &MainWindow::onRecognizeClicked);
    connect(m_closeImageBtn, &QPushButton::clicked, this, &MainWindow::onCloseImageClicked);
    connect(m_prevImageBtn, &QPushButton::clicked, this, [this]() {
        if (m_batchItems.isEmpty()) return;
        int total = m_batchItems.size();
        int current = (m_batchViewIndex >= 0) ? m_batchViewIndex : 0;
        int idx = current <= 0 ? total - 1 : current - 1;
        showBatchItem(idx);
    });
    connect(m_nextImageBtn, &QPushButton::clicked, this, [this]() {
        if (m_batchItems.isEmpty()) return;
        int total = m_batchItems.size();
        int current = (m_batchViewIndex >= 0) ? m_batchViewIndex : 0;
        int idx = (current + 1) % total;
        showBatchItem(idx);
    });
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

    // 历史记录事件
    connect(m_historyManager, &HistoryManager::historyChanged, this, [this]() {
        loadHistoryPage(1); // 历史变动时重置到第一页
    });
    
    connect(m_clearHistoryBtn, &QPushButton::clicked, this, &MainWindow::onClearHistoryClicked);

    connect(m_historyList, &QListWidget::itemClicked, this, &MainWindow::onHistoryItemClicked);
    
    // 筛选与分页事件
    connect(m_searchBtn, &QPushButton::clicked, this, [this]() { loadHistoryPage(1); });
    connect(m_prevPageBtn, &QPushButton::clicked, this, [this]() {
        if (m_historyPageNum > 1) loadHistoryPage(m_historyPageNum - 1);
    });
    connect(m_nextPageBtn, &QPushButton::clicked, this, [this]() {
        loadHistoryPage(m_historyPageNum + 1);
    });
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this]() { loadHistoryPage(1); });
    
    // 初始化加载第一页
    loadHistoryPage(1);

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
    // (已在 setupConnections 中移除旧的 historyChanged lambda)
    // m_historyManager = new HistoryManager(this);
    // connect(m_historyManager, &HistoryManager::historyChanged, [this]() { ... });
    
    m_historyManager = new HistoryManager(this);
    
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

    // 应用历史记录相关设置
    int maxHistory = m_configManager->getSetting("max_history", 50).toInt();
    m_historyManager->setMaxHistory(maxHistory);

    bool persistence = m_configManager->getSetting("history_persistence", false).toBool();
    m_historyManager->setPersistenceEnabled(persistence);

    // 每次启动都从数据库加载（若启用了持久化则会加载已保存记录）
    m_historyManager->loadHistory();

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
    reloadPromptTabs();

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

    updateBatchNav();
}

void MainWindow::startBatchProcessing(const QStringList& files, SubmitSource source)
{
    if (m_recognizing) {
        QMessageBox::information(this, "提示", "当前正在识别，请稍后再开始批量处理");
        return;
    }

    m_batchFiles.clear();
    m_batchItems.clear();
    m_batchViewIndex = -1;
    m_batchInFlight = 0;
    m_batchIndex = 0;
    m_batchPrompt = m_promptEdit ? m_promptEdit->toPlainText().trimmed() : QString();
    m_batchSource = source;
    m_batchRunning = true;

    int added = 0;
    int skipped = 0;
    for (const QString& path : files) {
        QImage img(path);
        if (img.isNull()) {
            skipped++;
            continue;
        }
        m_batchFiles << path;
        BatchItem item;
        item.path = path;
        item.image = img;
        m_batchItems.append(item);
        added++;
    }

    if (m_batchFiles.isEmpty()) {
        m_batchRunning = false;
        showStatusMessage("没有有效的图片可处理");
        updateBatchNav();
        return;
    }

    // 预览第一张
    m_batchViewIndex = 0;
    showBatchItem(0);

    if (skipped > 0) {
        showStatusMessage(QString("开始批量处理，共 %1 张（跳过 %2 张无效图片）").arg(added).arg(skipped));
    } else {
        showStatusMessage(QString("开始批量处理，共 %1 张").arg(added));
    }
    m_recognizing = true;
    m_recognizeBtn->setEnabled(false);
    setRecognizeButtonText("执行中...");
    updateBatchNav();
    dispatchBatchJobs();
}

void MainWindow::dispatchBatchJobs()
{
    if (!m_batchRunning)
        return;

    while (m_batchInFlight < kMaxBatchConcurrent && m_batchIndex < m_batchFiles.size()) {
        int idx = m_batchIndex++;
        if (idx >= m_batchItems.size())
            break;

        BatchItem& item = m_batchItems[idx];
        if (item.image.isNull()) {
            continue;
        }

        QString filePath = m_batchFiles.at(idx);
        QString hash;
        if (m_pipeline->currentAdapter()) {
            const auto& config = m_pipeline->currentAdapter()->config();
            hash = HistoryManager::computeContentHash(item.image, m_batchPrompt, config.id, config.params);
            
            // 缓存检查
            HistoryItem cached = m_historyManager->findItemByHash(hash);
            if (cached.result.success) {
                qDebug() << "Batch Cache Hit! Index:" << idx << "Hash:" << hash;
                
                // 更新批量项
                m_batchItems[idx].result = cached.result;
                m_batchItems[idx].finished = true;
                m_batchItems[idx].error.clear();
                
                // 添加到历史记录 (保持时间线更新)
                HistoryItem newItem = cached;
                newItem.timestamp = QDateTime::currentDateTime();
                newItem.source = m_batchSource;
                addHistoryItem(newItem);
                
                // 跳过提交，继续下一个
                continue;
            }
        }

        QString contextId = QString("batch:%1|hash:%2").arg(idx).arg(hash);
        m_batchInFlight++;

        showStatusMessage(QString("批量处理中 (%1/%2): %3")
                              .arg(idx + 1)
                              .arg(m_batchFiles.size())
                              .arg(QFileInfo(filePath).fileName()));
        m_pipeline->submitImage(item.image, m_batchSource, m_batchPrompt, contextId);
    }

    if (m_batchIndex >= m_batchFiles.size() && m_batchInFlight == 0) {
        m_batchRunning = false;
        showStatusMessage("批量处理完成");
        m_recognizing = false;
        m_recognizeBtn->setEnabled(true);
        setRecognizeButtonText("询问AI");
    }

    updateBatchNav();
}

void MainWindow::showBatchItem(int index)
{
    if (index < 0 || index >= m_batchItems.size())
        return;

    const BatchItem& item = m_batchItems.at(index);
    m_batchViewIndex = index;
    if (!item.image.isNull()) {
        loadImage(item.image, m_batchSource);
    }

    if (m_resultText) {
    if (item.finished) {
        if (item.result.success) {
            m_resultText->setPlainText(item.result.fullText);
        } else {
            m_resultText->setPlainText(item.error.isEmpty() ? "识别失败" : item.error);
        }
    } else {
        m_resultText->setPlainText("等待识别或处理中...");
    }

    updateBatchNav();
}
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
    if (m_prevImageBtn) m_prevImageBtn->hide();
    if (m_nextImageBtn) m_nextImageBtn->hide();
    if (m_batchInfoLabel) m_batchInfoLabel->hide();
    
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

    updateBatchNav();
}

void MainWindow::onUploadImageClicked()
{
    QStringList fileNames = QFileDialog::getOpenFileNames(
        this,
        "选择图片",
        QString(),
        "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif *.webp)");

    if (fileNames.isEmpty())
    {
        return;
    }

    // 单张图片：沿用原有逻辑
    if (fileNames.size() == 1)
    {
        QImage image(fileNames.first());
        if (image.isNull())
        {
            QMessageBox::warning(this, "错误", "无法加载图片文件");
            return;
        }
        // 清空批量状态
        m_batchRunning = false;
        m_batchFiles.clear();
        loadImage(image, SubmitSource::Upload);
        return;
    }

    // 多张图片：启用批量处理
    startBatchProcessing(fileNames, SubmitSource::Upload);
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
    // 手动点击识别时，视为单次任务，清理批量状态
    m_batchRunning = false;
    m_batchFiles.clear();
    m_batchIndex = 0;
    m_batchItems.clear();
    m_batchViewIndex = -1;
    m_batchInFlight = 0;
    if (m_batchInfoLabel) m_batchInfoLabel->hide();
    if (m_prevImageBtn) m_prevImageBtn->hide();
    if (m_nextImageBtn) m_nextImageBtn->hide();

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
        
        QString baseMsg = QString("模型未初始化！\n模型：%1\n\n").arg(modelName);
        QString detailMsg;

        if (engine == "tesseract") {
            detailMsg = "请检查 Tesseract 是否正确安装：\n"
                        "1. 下载安装 Tesseract-OCR\n"
                        "2. 添加到系统 PATH\n"
                        "3. 或在设置中指定完整路径";
        } 
        else if (engine == "qwen" || engine == "custom" || engine == "gen" || 
                 engine == "gemini" || engine == "glm" || engine == "paddle" || 
                 engine == "doubao") 
        {
            detailMsg = "可能的原因：\n"
                        "1. API Key 未配置或无效\n"
                        "2. API URL 未配置\n"
                        "3. 网络连接问题\n\n"
                        "请点击设置按钮，在「模型配置」标签页填写 API Key 和 API URL";
        } 
        else 
        {
            detailMsg = "请检查模型配置。";
        }
        
        errorMsg = baseMsg + detailMsg;
        
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
    QString hash;
    if (m_pipeline->currentAdapter()) {
        const auto& config = m_pipeline->currentAdapter()->config();
        hash = HistoryManager::computeContentHash(imageToSubmit, prompt, config.id, config.params);
        
        // 缓存检查
        HistoryItem cached = m_historyManager->findItemByHash(hash);
        if (cached.result.success) {
            qDebug() << "Cache Hit! Hash:" << hash;
            OCRResult result = cached.result;
            result.timestamp = QDateTime::currentDateTime();
            result.processingTimeMs = 0; // 标识为缓存结果
            
            onRecognitionCompleted(result, imageToSubmit, SubmitSource::Upload, "hash:" + hash);
            showStatusMessage("命中缓存，直接返回结果");
            return;
        }
    }

    m_pipeline->submitImage(imageToSubmit, SubmitSource::Upload, prompt, "hash:" + hash);
    
    // 重新应用当前主题样式，防止折叠/展开状态下按钮图标错位
    applyTheme(m_isGrayTheme);
}

void MainWindow::onClearHistoryClicked()
{
    // 使用 getTotalCount 判断是否有记录
    if (m_historyManager->getTotalCount() == 0)
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
        m_historyManager->clearHistory();
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
    dialog.resize(960, 640);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    QSplitter* splitter = new QSplitter(Qt::Horizontal, &dialog);
    splitter->setHandleWidth(6);
    splitter->setStyleSheet(
        "QSplitter::handle { background: #f0f0f0; border: 1px solid #e0e0e0; }"
        "QSplitter::handle:hover { background: #e6e6e6; }"
    );
    
    QTextEdit* editor = new QTextEdit(splitter);
    editor->setPlainText(currentText);
    editor->setLineWrapMode(QTextEdit::NoWrap);
    QFont codeFont("Consolas");
    codeFont.setPointSize(11);
    editor->setFont(codeFont);
    editor->setStyleSheet(
        "QTextEdit { "
        "  background: #ffffff; "
        "  border: 1px solid #dcdcdc; "
        "  border-radius: 8px; "
        "  padding: 12px; "
        "  color: #1f1f1f; "
        "  selection-background-color: #d0e7ff; "
        "  selection-color: #000000; "
        "}"
    );
    
    QTextBrowser* preview = new QTextBrowser(splitter);
    preview->setOpenExternalLinks(true);
    preview->setMarkdown(currentText);
    QFont bodyFont = preview->font();
    bodyFont.setPointSize(11);
    preview->setFont(bodyFont);
    preview->setStyleSheet(
        "QTextBrowser { "
        "  background: #fafafa; "
        "  border: 1px solid #dcdcdc; "
        "  border-radius: 8px; "
        "  padding: 12px; "
        "  color: #1f1f1f; "
        "}"
    );
    preview->document()->setDefaultStyleSheet(
        "body { font-family: 'Segoe UI','Microsoft YaHei','Helvetica',sans-serif; "
        "       font-size: 11pt; color: #1f1f1f; line-height: 1.6; }"
        "h1 { font-size: 18pt; margin: 0 0 12px 0; }"
        "h2 { font-size: 16pt; margin: 0 0 10px 0; }"
        "h3 { font-size: 14pt; margin: 0 0 8px 0; }"
        "p { margin: 0 0 10px 0; }"
        "ul, ol { padding-left: 22px; margin: 6px 0; }"
        "li { margin: 4px 0; }"
        "code { font-family: 'Consolas','Menlo','Monaco','Courier New',monospace; "
        "       background: #f0f0f0; padding: 2px 4px; border-radius: 4px; }"
        "pre { background: #f5f5f5; border: 1px solid #e0e0e0; border-radius: 6px; "
        "      padding: 10px; overflow: auto; }"
        "pre code { background: transparent; padding: 0; }"
        "blockquote { color: #555; border-left: 4px solid #dddddd; padding-left: 10px; margin-left: 0; }"
        "a { color: #0078d7; text-decoration: none; }"
        "a:hover { text-decoration: underline; }"
        "img { max-width: 100%; }"
        );
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

    // 定义支持的格式
    QString filters = "纯文本 (*.txt);;Markdown (*.md);;PDF 文档 (*.pdf);;Word 文档 (*.doc);;Excel 表格 (*.csv)";
    QString selectedFilter;

    QString fileName = QFileDialog::getSaveFileName(
        this,
        "导出结果",
        QString("ocr_result_%1.txt").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")),
        filters,
        &selectedFilter);

    if (fileName.isEmpty())
    {
        return;
    }

    QString content = m_resultText->toPlainText();
    
    // 根据文件扩展名处理导出
    if (fileName.endsWith(".pdf", Qt::CaseInsensitive))
    {
        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(fileName);
        
        QTextDocument doc;
        // 使用简单的 HTML 包装来保持换行
        QString htmlContent = QString("<pre>%1</pre>").arg(content.toHtmlEscaped());
        doc.setHtml(htmlContent);
        doc.print(&printer);
    }
    else if (fileName.endsWith(".doc", Qt::CaseInsensitive))
    {
        // 导出为 HTML 伪装的 Word 文档
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QMessageBox::warning(this, "错误", "无法写入文件");
            return;
        }
        QTextStream out(&file);
        out.setCodec("UTF-8");
        // 添加 Word 友好的 HTML 头
        out << "<html xmlns:o='urn:schemas-microsoft-com:office:office' xmlns:w='urn:schemas-microsoft-com:office:word' xmlns='http://www.w3.org/TR/REC-html40'>";
        out << "<head><meta charset='utf-8'><title>OCR Result</title></head><body>";
        out << QString("<pre style='font-family: Arial; font-size: 11pt;'>%1</pre>").arg(content.toHtmlEscaped());
        out << "</body></html>";
        file.close();
    }
    else if (fileName.endsWith(".csv", Qt::CaseInsensitive))
    {
        // 导出为 CSV
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QMessageBox::warning(this, "错误", "无法写入文件");
            return;
        }
        QTextStream out(&file);
        // UTF-8 BOM 以兼容 Excel
        out.setCodec("UTF-8");
        out.setGenerateByteOrderMark(true);
        
        // 简单的 CSV 转换：每行作为一行
        QStringList lines = content.split('\n');
        for(const QString& line : lines) {
            // CSV 转义：如果有逗号或引号，用双引号包裹，并双写内部引号
            QString csvLine = line;
            if (csvLine.contains(',') || csvLine.contains('"') || csvLine.contains('\n')) {
                csvLine.replace("\"", "\"\"");
                csvLine = "\"" + csvLine + "\"";
            }
            out << csvLine << "\n";
        }
        file.close();
    }
    else
    {
        // 默认文本导出 (txt, md)
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QMessageBox::warning(this, "错误", "无法写入文件");
            return;
        }

        QTextStream out(&file);
        out.setCodec("UTF-8");
        out << content;
        file.close();
    }

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

void MainWindow::onRecognitionStarted(const QImage &image, SubmitSource source, const QString& contextId)
{
    Q_UNUSED(image);
    Q_UNUSED(source);
    Q_UNUSED(contextId);

    qDebug() << "========================================";
    qDebug() << "UI: 识别任务开始";
    qDebug() << "========================================";

    m_recognizing = true;
    m_recognizeBtn->setEnabled(false);
    setRecognizeButtonText("执行中...");
    showStatusMessage("正在执行...");
}

void MainWindow::onRecognitionCompleted(const OCRResult &result, const QImage &image, SubmitSource source, const QString& contextId)
{
    if (!result.success)
    {
        onRecognitionFailed(result.errorMessage, image, source, contextId);
        return;
    }

    // 添加到历史
    HistoryItem item;
    item.image = image;
    item.result = result;
    item.source = source;
    item.timestamp = result.timestamp;
    
    // 解析 ContextID 获取 Hash 和 BatchIndex
    int batchIdx = -1;
    QStringList parts = contextId.split('|');
    for (const QString& part : parts) {
        if (part.startsWith("hash:")) {
            item.contentHash = part.mid(5);
        } else if (part.startsWith("batch:")) {
            bool ok = false;
            batchIdx = part.mid(6).toInt(&ok);
            if (!ok) batchIdx = -1;
        }
    }

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

    // 批量任务则继续下一张
    if (m_batchRunning) {
        if (batchIdx >= 0 && batchIdx < m_batchItems.size()) {
            m_batchItems[batchIdx].result = result;
            m_batchItems[batchIdx].finished = true;
            m_batchItems[batchIdx].error.clear();
        }
        m_batchInFlight = qMax(0, m_batchInFlight - 1);
        dispatchBatchJobs();
        bool busy = m_batchRunning && (m_batchInFlight > 0 || m_batchIndex < m_batchFiles.size());
        m_recognizing = busy;
        m_recognizeBtn->setEnabled(!busy);
        setRecognizeButtonText(busy ? "执行中..." : "询问AI");
        return;
    }

    m_recognizing = false;
    m_recognizeBtn->setEnabled(true);
    setRecognizeButtonText("询问AI");
}

void MainWindow::onRecognitionFailed(const QString &error, const QImage &image, SubmitSource source, const QString& contextId)
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

    // 批量任务失败后继续下一张
    if (m_batchRunning) {
        int batchIdx = -1;
        QStringList parts = contextId.split('|');
        for (const QString& part : parts) {
            if (part.startsWith("batch:")) {
                bool ok = false;
                batchIdx = part.mid(6).toInt(&ok);
                if (!ok) batchIdx = -1;
                break;
            }
        }

        if (batchIdx >= 0 && batchIdx < m_batchItems.size()) {
            m_batchItems[batchIdx].finished = true;
            m_batchItems[batchIdx].error = error;
            m_batchItems[batchIdx].result.success = false;
            m_batchItems[batchIdx].result.errorMessage = error;
        }
        m_batchInFlight = qMax(0, m_batchInFlight - 1);
        dispatchBatchJobs();
        bool busy = m_batchRunning && (m_batchInFlight > 0 || m_batchIndex < m_batchFiles.size());
        m_recognizing = busy;
        m_recognizeBtn->setEnabled(!busy);
        setRecognizeButtonText(busy ? "执行中..." : "询问AI");
        return;
    }

    m_recognizing = false;
    m_recognizeBtn->setEnabled(true);
    setRecognizeButtonText("询问AI");
}

void MainWindow::addHistoryItem(const HistoryItem &item)
{
    m_historyManager->addHistoryItem(item);
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

void MainWindow::loadHistoryPage(int page)
{
    if (page < 1) page = 1;
    m_historyPageNum = page;
    m_pageLabel->setText(QString("第 %1 页").arg(page));
    
    // 构建筛选条件
    HistoryManager::HistoryFilter filter;
    filter.startTime = m_startDateEdit->dateTime();
    // 结束时间设为当天的23:59:59
    QDateTime end = m_endDateEdit->dateTime();
    end.setTime(QTime(23, 59, 59));
    filter.endTime = end;
    filter.keyword = m_searchEdit->text().trimmed();
    
    // 获取数据
    int total = m_historyManager->getTotalCount(filter);
    int totalPages = (total + m_historyPageSize - 1) / m_historyPageSize;
    if (totalPages < 1) totalPages = 1;
    
    // 更新分页按钮状态
    m_prevPageBtn->setEnabled(page > 1);
    m_nextPageBtn->setEnabled(page < totalPages);
    m_pageLabel->setText(QString("第 %1 / %2 页 (共 %3 条)").arg(page).arg(totalPages).arg(total));
    
    QVector<HistoryItem> list = m_historyManager->getHistoryList(page, m_historyPageSize, filter);
    
    m_historyList->clear();
    for (const auto& item : list) {
        QString timeStr = item.timestamp.toString("yyyy-MM-dd HH:mm");
        QString preview = item.result.fullText.left(50).replace("\n", " ");
        if (preview.isEmpty()) preview = "[无文字]";
        else if (item.result.fullText.length() > 50) preview += "...";
        
        QString modelInfo = item.result.modelName;
        if (modelInfo.isEmpty()) modelInfo = "Unknown";
        
        // 自定义列表项显示格式
        QString displayText = QString("%1 | %2\n%3").arg(timeStr, modelInfo, preview);
        
        QListWidgetItem* listItem = new QListWidgetItem(displayText);
        // 存储ID以便点击时查询详情
        listItem->setData(Qt::UserRole, item.id);
        m_historyList->addItem(listItem);
    }
}

void MainWindow::onHistoryItemClicked(QListWidgetItem *item)
{
    long long id = item->data(Qt::UserRole).toLongLong();
    HistoryItem detail = m_historyManager->getHistoryDetail(id);
    
    if (detail.id != -1) // 有效记录
    {
        // 确保 m_currentHistoryIndex 逻辑不再依赖旧的 index，而是直接展示
        updateResultDisplay(detail);
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
    bool settingsWereChanged = false;
    
    // 连接信号，标记设置是否被更改
    connect(&dialog, &SettingsDialog::settingsChanged, this, [&settingsWereChanged]() {
        settingsWereChanged = true;
    });
    
    dialog.exec();

    // 关闭设置后重新注册快捷键
    setupShortcuts();
    
    // 如果设置被更改，延迟处理（确保对话框完全关闭后再处理）
    if (settingsWereChanged) {
        QTimer::singleShot(100, this, &MainWindow::onSettingsChanged);
    }
}

// onPromptTemplateSelected 函数已移除，改为在列表项点击时直接处理

void MainWindow::onSettingsChanged()
{
    // 防止重复执行：如果正在处理设置变更，直接返回
    if (m_settingsChanging) {
        qWarning() << "设置变更处理正在进行中，忽略重复调用";
        return;
    }
    
    // 安全检查：确保关键对象存在
    if (!m_configManager || !m_modelManager || !m_historyManager || !m_pipeline) {
        qCritical() << "设置变更处理失败：关键对象未初始化";
        return;
    }
    
    m_settingsChanging = true;
    qDebug() << "设置已更改，重新加载配置";
    
    try {
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
        
        // 应用历史记录持久化设置
        bool persistence = m_configManager->getSetting("history_persistence", false).toBool();
        if (m_historyManager) {
            m_historyManager->setPersistenceEnabled(persistence);
        }

        // 应用最大历史记录数量设置
        int maxHistory = m_configManager->getSetting("max_history", 50).toInt();
        if (m_historyManager) {
            m_historyManager->setMaxHistory(maxHistory);
        }

        // 清空现有模型（安全地删除）
        if (m_modelManager) {
            QList<ModelAdapter*> oldModels = m_modelManager->getAllModels();
            for (ModelAdapter* adapter : oldModels) {
                if (adapter) {
                    QString modelId = adapter->config().id;
                    m_modelManager->removeModel(modelId);
                }
            }
        }
    
        // 临时保存要恢复的模型ID（在添加模型前，因为addModel会自动激活第一个模型）
        QString modelIdToRestore = currentModelId;
        
        // 重新创建模型（注意：addModel会自动将第一个模型设为激活）
        if (!m_modelManager) {
            qCritical() << "ModelManager 未初始化，无法重新创建模型";
            m_settingsChanging = false;
            return;
        }
        
        QList<ModelConfig> configs = m_configManager->getModelConfigs();
        for (const ModelConfig& config : configs) {
            if (!config.enabled) {
                continue;
            }
            
            ModelAdapter* adapter = nullptr;
            try {
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
                
                if (adapter && m_modelManager) {
                    m_modelManager->addModel(adapter);
                }
            } catch (...) {
                qWarning() << "创建模型适配器失败:" << config.id;
                if (adapter) {
                    delete adapter;
                    adapter = nullptr;
                }
            }
        }
        
        // 重新初始化模型
        if (m_modelManager) {
            m_modelManager->initializeAll();
        }
        
        // 尝试恢复之前选择的模型，如果不存在则保持当前激活的模型（第一个模型）
        if (!modelIdToRestore.isEmpty() && m_modelManager) {
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
        } else if (m_modelManager) {
            // 如果没有保存的模型ID，保持当前激活的模型（通常是第一个）
            ModelAdapter* currentActive = m_modelManager->getActiveModel();
            if (currentActive) {
                qDebug() << "没有保存的模型ID，保持当前激活模型:" << currentActive->config().id << "(" << currentActive->config().displayName << ")";
            }
        }
        
        // 更新 UI（临时断开信号连接，避免触发模型切换）
        if (m_modelComboBox) {
            disconnect(m_modelComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                       this, &MainWindow::onModelChanged);
            
            m_modelComboBox->clear();
            if (m_modelManager) {
                for (ModelAdapter* adapter : m_modelManager->getAllModels()) {
                    if (adapter) {
                        QString itemText = adapter->config().displayName;
                        if (!adapter->isInitialized()) {
                            itemText += " [未初始化]";
                        }
                        m_modelComboBox->addItem(itemText, adapter->config().id);
                        int idx = m_modelComboBox->count() - 1;
                        m_modelComboBox->setItemData(idx, itemText, Qt::ToolTipRole); // 悬浮显示完整信息
                    }
                }
            }
            
            // 同步当前激活模型到下拉框与 pipeline
            if (m_modelManager && m_pipeline) {
                ModelAdapter* active = m_modelManager->getActiveModel();
                if (active) {
                    m_pipeline->setCurrentAdapter(active);
                    const QString activeId = active->config().id;
                    for (int i = 0; i < m_modelComboBox->count(); ++i) {
                        if (m_modelComboBox->itemData(i).toString() == activeId) {
                            m_modelComboBox->setCurrentIndex(i);
                            break;
                        }
                    }
                    QString typeText = active->config().type == "local" ? "离线" : "线上";
                    if (m_statusLabel) {
                        m_statusLabel->setText(QString("就绪 - %1 (%2)").arg(active->config().displayName, typeText));
                    }
                }
            }
            
            // 重新连接信号
            connect(m_modelComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, &MainWindow::onModelChanged);
        }
        
        // 重新加载提示词模板选项卡
        reloadPromptTabs();
        
        // 重新注册快捷键
        setupShortcuts();
        
    } catch (...) {
        qCritical() << "设置变更处理过程中发生异常";
    }
    
    // 重置标志，允许下次设置变更
    m_settingsChanging = false;
    qDebug() << "设置变更处理完成";
}

void MainWindow::onSidebarCollapseChanged(bool collapsed)
{
    m_sidebarCollapsed = collapsed;
    
    // 更新分割器大小
    if (m_mainSplitter) {
        if (m_sidebarCollapsed) {
            m_mainSplitter->setSizes({60, 1000});
        } else {
            m_mainSplitter->setSizes({300, 900});
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

    // 1) 数据源：配置 -> 缓存（空时回退），首次成功锁定基线，之后始终使用基线保证数量稳定
    QList<PromptTemplate> templates = m_configManager->getPromptTemplates();
    if (templates.isEmpty() && !m_promptTemplatesCache.isEmpty()) {
        templates = m_promptTemplatesCache.toList();
    } else {
        m_promptTemplatesCache = QVector<PromptTemplate>::fromList(templates);
    }
    if (m_promptTemplatesBaseline.isEmpty() && !templates.isEmpty()) {
        m_promptTemplatesBaseline = QVector<PromptTemplate>::fromList(templates);
    }
    if (!m_promptTemplatesBaseline.isEmpty()) {
        templates = m_promptTemplatesBaseline.toList();
        m_promptTemplatesCache = m_promptTemplatesBaseline;
    }

    // 2) 样式
    const bool grayTheme = m_isGrayTheme;
    const QString scrollBarStyle = grayTheme
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

    const QString listStyle = grayTheme
        ? "QListWidget { border: none; background: #2b2b2b; color: #e0e0e0; padding: 4px; }"
          "QListWidget::item { padding: 10px 12px; border-bottom: 1px solid rgba(255, 255, 255, 0.08); border-radius: 4px; }"
          "QListWidget::item:selected { background: rgba(255, 255, 255, 0.12); color: #ffffff; }"
          "QListWidget::item:hover { background: rgba(255, 255, 255, 0.08); }"
          "QListWidget::item:selected:hover { background: rgba(255, 255, 255, 0.12); }"
        : "QListWidget { border: none; background: transparent; color: #000000; padding: 4px; }"
          "QListWidget::item { padding: 8px 10px; border-bottom: 1px solid #f0f0f0; border-radius: 4px; }"
          "QListWidget::item:selected { background: #e0e0e0; color: #000000; }"
          "QListWidget::item:hover { background: #f5f5f5; }"
          "QListWidget::item:selected:hover { background: #e0e0e0; }";

    // 3) 清空并重建 Tab
    while (m_promptCategoryTabs->count() > 0) m_promptCategoryTabs->removeTab(0);
    m_promptTypeLists.clear();

    const QStringList typeOrder = {"识别", "翻译", "解答", "整理"};
    QMap<QString, QList<PromptTemplate>> groups;
    for (const QString& t : typeOrder) groups[t] = {};
    for (const PromptTemplate& tmpl : templates) 
    {
        groups[tmpl.type].append(tmpl);
    }

    auto addTab = [&](const QString& type, const QList<PromptTemplate>& data) {
        QWidget* tab = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(tab);
        layout->setContentsMargins(0, 0, 0, 0);

        QListWidget* list = new QListWidget();
        list->setStyleSheet(listStyle);
        list->verticalScrollBar()->setStyleSheet(scrollBarStyle);
        list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        list->setMinimumHeight(240);
        layout->addWidget(list);

        m_promptCategoryTabs->addTab(tab, type);
        m_promptTypeLists[type] = list;

        for (const PromptTemplate& tmpl : data) {
            auto* item = new QListWidgetItem(QString("%1 [%2]").arg(tmpl.name, tmpl.category));
            item->setData(Qt::UserRole, tmpl.content);
            item->setData(Qt::UserRole + 1, tmpl.name);
            list->addItem(item);
        }

        connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
            if (!item || !m_promptEdit) return;
            const QString content = item->data(Qt::UserRole).toString();
            if (!content.isEmpty()) {
                m_promptEdit->setPlainText(content);
                qDebug() << "选择提示词模板:" << item->data(Qt::UserRole + 1).toString();
            }
        });
    };

    for (const QString& type : typeOrder) addTab(type, groups.value(type));
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        if (!typeOrder.contains(it.key())) addTab(it.key(), it.value());
    }

    // 4) 默认选中（优先“识别”中的“通用识别”，否则首项）
    if (m_promptTypeLists.contains("识别")) {
        m_promptCategoryTabs->setCurrentIndex(typeOrder.indexOf("识别"));
        QListWidget* list = m_promptTypeLists["识别"];
        QListWidgetItem* targetItem = nullptr;
        for (int i = 0; list && i < list->count(); ++i) {
            QListWidgetItem* item = list->item(i);
            if (item && item->data(Qt::UserRole + 1).toString().contains("通用识别")) {
                targetItem = item;
                break;
            }
        }
        if (!targetItem && list && list->count() > 0) targetItem = list->item(0);
        if (list && targetItem) {
            list->setCurrentItem(targetItem);
            const QString content = targetItem->data(Qt::UserRole).toString();
            if (!content.isEmpty() && m_promptEdit) m_promptEdit->setPlainText(content);
        }
    } else if (!m_promptTypeLists.isEmpty()) {
        m_promptCategoryTabs->setCurrentIndex(0);
        QListWidget* firstList = m_promptTypeLists.begin().value();
        if (firstList && firstList->count() > 0) {
            firstList->setCurrentRow(0);
            const QString content = firstList->item(0)->data(Qt::UserRole).toString();
            if (!content.isEmpty() && m_promptEdit) m_promptEdit->setPlainText(content);
        }
    }
}

void MainWindow::applyTheme(bool grayTheme)
{
    ThemeManager::applyTheme(this, grayTheme);
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
        QStringList files;
        for (const QUrl& url : urls) {
            QString path = url.toLocalFile();
            if (!path.isEmpty())
                files << path;
        }
        if (files.isEmpty())
            return;

    if (files.size() == 1) {
        QImage image(files.first());
        if (!image.isNull())
        {
            loadImage(image, SubmitSource::DragDrop);
            event->acceptProposedAction();
        }
    } else {
        startBatchProcessing(files, SubmitSource::DragDrop);
        event->acceptProposedAction();
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

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    // 使用异步调用，确保布局更新后再定位浮动控件
    QTimer::singleShot(0, this, [this]() {
        updateCloseButtonPosition();
        updateBatchNav();
    });
}

// ScreenshotSelector 类已移至 ScreenshotSelector.h/cpp

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
void MainWindow::updateBatchNav()
{
    if (!m_imageContainer)
        return;

    int total = m_batchItems.size();
    bool showNav = total > 1;

    if (m_prevImageBtn && m_nextImageBtn) {
        m_prevImageBtn->setVisible(showNav);
        m_nextImageBtn->setVisible(showNav);

        if (showNav) {
            int midY = m_imageContainer->height() / 2 - m_prevImageBtn->height() / 2;
            m_prevImageBtn->move(10, qMax(10, midY));
            m_nextImageBtn->move(m_imageContainer->width() - m_nextImageBtn->width() - 10, qMax(10, midY));
            m_prevImageBtn->raise();
            m_nextImageBtn->raise();
        }
    }

    if (m_batchInfoLabel) {
        if (total > 0) {
            QString status;
            if (m_batchViewIndex >= 0 && m_batchViewIndex < m_batchItems.size()) {
                const BatchItem& item = m_batchItems[m_batchViewIndex];
                QString fileName = QFileInfo(item.path).fileName();
                if (!item.finished) {
                    status = QString("第 %1/%2 张 · %3 · 处理中/待处理").arg(m_batchViewIndex + 1).arg(total).arg(fileName);
                } else if (item.result.success) {
                    status = QString("第 %1/%2 张 · %3 · 完成").arg(m_batchViewIndex + 1).arg(total).arg(fileName);
                } else {
                    status = QString("第 %1/%2 张 · %3 · 失败").arg(m_batchViewIndex + 1).arg(total).arg(fileName);
                }
            }
            m_batchInfoLabel->setText(status);
            m_batchInfoLabel->setVisible(true);
            m_batchInfoLabel->move(10, 10);
            m_batchInfoLabel->raise();
        } else {
            m_batchInfoLabel->hide();
        }
    }
}