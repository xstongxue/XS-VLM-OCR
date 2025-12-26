#pragma once
#include <QMainWindow>
#include <QListWidget>
#include <QTextEdit>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QSystemTrayIcon>
#include <QMenuBar>
#include <QMenu>
#include <QTimer>
#include <QShortcut>
#include <QCloseEvent>
#include "../core/OCRPipeline.h"
#include "../managers/ModelManager.h"
#include "../managers/ClipboardManager.h"
#include "../utils/ConfigManager.h"

#ifdef _WIN32
#include <windows.h>
#endif

// 识别历史记录项
struct HistoryItem {
    QImage image;
    OCRResult result;
    SubmitSource source;
    QDateTime timestamp;
};

// 主窗口
class MainWindow : public QMainWindow {
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;
    
protected:
    // 拖拽支持
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    // 粘贴支持
    void keyPressEvent(QKeyEvent* event) override;
    // 关闭按钮逻辑：隐藏到托盘
    void closeEvent(QCloseEvent* event) override;
    
#ifdef _WIN32
    // Windows全局热键支持
    bool nativeEvent(const QByteArray& eventType, void* message, long* result) override;
#endif
    
private slots:
    // UI 事件
    void onUploadImageClicked();
    void onPasteImageClicked();
    void onRecognizeClicked();
    void onPreviewResultClicked();
    void onCloseImageClicked(); 
    void onClearHistoryClicked();
    void onModelChanged(int index);
    void onExportResultClicked();
    void onSettingsClicked();
    
    // OCR Pipeline 回调
    void onRecognitionStarted(const QImage& image, SubmitSource source);
    void onRecognitionCompleted(const OCRResult& result, const QImage& image, SubmitSource source);
    void onRecognitionFailed(const QString& error, const QImage& image, SubmitSource source);
    
    // 历史列表选择
    void onHistoryItemClicked(QListWidgetItem* item);
    // 系统托盘
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    // 设置变更
    void onSettingsChanged();
    // 主题切换
    void onThemeToggleClicked();
    // 侧边栏折叠/展开
    void onSidebarToggleClicked();
    // 重新加载提示词选项卡
    void reloadPromptTabs();
    // 快捷键
    void disableShortcuts();
    void onScreenshotShortcut();
    void onRecognizeShortcut();
    
private:
    // 初始化 UI
    void setupUI();
    void setupConnections();
    void setupSystemTray();
    void setupShortcuts();
    void updateButtonTooltips(); // 更新按钮的 tooltip（包含快捷键）
    void applyTheme(bool grayTheme);
    void setRecognizeButtonText(const QString& text); // 根据折叠状态设置“询问AI”按钮文字
    
    // 初始化后台服务
    void initializeServices();
    // 加载图像
    void loadImage(const QImage& image, SubmitSource source);
    // 添加历史记录
    void addHistoryItem(const HistoryItem& item);
    // 更新结果显示
    void updateResultDisplay(const HistoryItem& item);
    // 更新关闭按钮位置
    void updateCloseButtonPosition();
    // 显示状态消息
    void showStatusMessage(const QString& message, int timeout = 3000);

    // UI 辅助方法
    QWidget* createCard(const QString& title, QWidget* content);
    void switchToPage(const QString& page);
    // UI 组件
    QWidget* m_centralWidget;
    QWidget* m_mainContainer;
    // 左侧：导航栏
    QWidget* m_sidebarPanel;
    QPushButton* m_sidebarToggleBtn;  // 折叠/展开按钮
    QPushButton* m_navHomeBtn;
    QPushButton* m_navHistoryBtn;
    QPushButton* m_navSettingsBtn;
    // 右侧：内容区（使用 StackedWidget 切换不同页面）
    QWidget* m_contentPanel;
    QStackedWidget* m_stackedWidget;
    QWidget* m_homePage;
    QWidget* m_historyPage;
    QWidget* m_modelsPage;
    // 首页组件
    QLabel* m_currentModelCard;
    QLabel* m_statsCard;
    QPushButton* m_themeToggleBtn;
    QComboBox* m_modelComboBox;
    QTabWidget* m_promptCategoryTabs;  // 提示词分类选项卡
    QMap<QString, QListWidget*> m_promptTypeLists;  // 按类型分组的提示词列表
    QTextEdit* m_promptEdit;
    QPushButton* m_uploadBtn;
    QPushButton* m_pasteBtn;
    QPushButton* m_recognizeBtn;
    // 图像显示
    QLabel* m_imageLabel;
    QPushButton* m_closeImageBtn;  // 关闭图片按钮
    QWidget* m_imageContainer;     // 图片容器（用于关闭按钮定位）
    QImage m_currentImage;
    // 结果显示
    QTextEdit* m_resultText;
    QPushButton* m_previewResultBtn;
    QPushButton* m_copyResultBtn;
    QPushButton* m_exportBtn;
    // 历史页面组件
    QListWidget* m_historyList;
    QPushButton* m_clearHistoryBtn;
    // 设置按钮
    QPushButton* m_settingsBtn;
    QLabel* m_statusLabel;
    // 系统托盘
    QSystemTrayIcon* m_trayIcon;
    QMenu* m_trayMenu;
    // 后台服务
    ModelManager* m_modelManager;
    OCRPipeline* m_pipeline;
    ClipboardManager* m_clipboardManager;
    ConfigManager* m_configManager;
    // 快捷键
    QShortcut* m_screenshotShortcut;
    QShortcut* m_recognizeShortcut;
    
#ifdef _WIN32
    // Windows全局热键ID
    int m_screenshotHotKeyId;
    int m_recognizeHotKeyId;
    static int s_hotKeyIdCounter;
#endif
    
    // 数据
    QVector<HistoryItem> m_history;
    int m_currentHistoryIndex;
    bool m_recognizing;
    bool m_isGrayTheme;
    bool m_sidebarCollapsed;  // 侧边栏是否折叠
    QSplitter* m_mainSplitter;  // 主分割器

    // 托盘提示是否已展示（避免重复弹出）
    bool m_trayNotified;
};
