#include "SettingsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QIcon>
#include <QDebug>

// ==================== SettingsDialog ====================
SettingsDialog::SettingsDialog(ConfigManager* configManager, QWidget* parent)
    : QDialog(parent)
    , m_configManager(configManager)
    , m_modified(false)
{
    setWindowTitle("设置");
    resize(900, 700);
    
    // 设置对话框整体样式（黑白简洁风格）
    setStyleSheet(
        "QDialog { "
        "  background-color: #ffffff; "
        "}"
        "QTabWidget::pane { "
        "  border: 1px solid #e0e0e0; "
        "  background: #ffffff; "
        "}"
        "QTabBar::tab { "
        "  margin-right: 2px;"
        "  background: #f5f5f5; "
        "  color: #666666; "
        "  padding: 10px 20px; "
        "  border: 1px solid #e0e0e0; "
        "  border-bottom: none; "
        "  border-top-left-radius: 4px; "
        "  border-top-right-radius: 4px; "
        "}"
        "QTabBar::tab:selected { "
        "  background: #ffffff; "
        "  color: #000000; "
        "  font-weight: normal; "
        "}"
        "QTabBar::tab:hover { "
        "  background: #e0e0e0; "
        "}"
        "QPushButton { "
        "  padding: 8px 16px; "
        "  font-size: 10pt; "
        "  font-weight: normal; "
        "  color: #000000; "
        "  background: #f5f5f5; "
        "  border: 1px solid #e0e0e0; "
        "  border-radius: 4px; "
        "}"
        "QPushButton:hover { "
        "  background: #e0e0e0; "
        "}"
        "QPushButton:pressed { "
        "  background: #d0d0d0; "
        "}"
        "QLineEdit, QTextEdit, QComboBox { "
        "  padding: 6px 10px; "
        "  font-size: 10pt; "
        "  border: 1px solid #e0e0e0; "
        "  border-radius: 4px; "
        "  background: #ffffff; "
        "  color: #000000; "
        "}"
        "QLineEdit:focus, QTextEdit:focus, QComboBox:focus { "
        "  border: 1px solid #000000; "
        "}"
        "QListWidget { "
        "  border: 1px solid #e0e0e0; "
        "  border-radius: 4px; "
        "  background: #ffffff; "
        "  color: #000000; "
        "}"
        "QListWidget::item { "
        "  padding: 8px; "
        "  border-bottom: 1px solid #f0f0f0; "
        "}"
        "QListWidget::item:selected { "
        "  background: #f5f5f5; "
        "  color: #000000; "
        "}"
        "QListWidget::item:hover { "
        "  background: #f0f0f0; "
        "}"
        "QGroupBox { "
        "  font-weight: normal; "
        "  font-size: 11pt; "
        "  border: 1px solid #e0e0e0; "
        "  border-radius: 8px; "
        "  margin-top: 12px; "
        "  padding-top: 12px; "
        "  background-color: #ffffff; "
        "  color: #000000; "
        "}"
        "QGroupBox::title { "
        "  subcontrol-origin: margin; "
        "  left: 10px; "
        "  padding: 0 5px; "
        "  color: #000000; "
        "}"
        "QCheckBox { "
        "  color: #000000; "
        "  font-size: 10pt; "
        "}"
        "QCheckBox::indicator { "
        "  width: 18px; "
        "  height: 18px; "
        "  border: 1px solid #e0e0e0; "
        "  border-radius: 3px; "
        "  background: #ffffff; "
        "}"
        "QCheckBox::indicator:checked { "
        "  background: #000000; "
        "  border: 1px solid #000000; "
        "}"
        "QSpinBox { "
        "  padding: 6px 10px; "
        "  border: 1px solid #e0e0e0; "
        "  border-radius: 4px; "
        "  background: #ffffff; "
        "  color: #000000; "
        "}"
        "QKeySequenceEdit { "
        "  padding: 6px 10px; "
        "  border: 1px solid #e0e0e0; "
        "  border-radius: 4px; "
        "  background: #ffffff; "
        "  color: #000000; "
        "}"
    );
    
    setupUI();
    loadSettings();
}

void SettingsDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // 创建标签页
    m_tabWidget = new QTabWidget(this);
    setupModelTab();
    setupPromptTab();
    setupShortcutTab();
    setupGeneralTab();
    setupAboutTab();
    
    mainLayout->addWidget(m_tabWidget);
    
    // 底部按钮
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    m_saveBtn = new QPushButton("保存", this);
    m_cancelBtn = new QPushButton("取消", this);
    m_applyBtn = new QPushButton("应用", this);
    
    // 底部按钮样式（深色按钮）
    m_saveBtn->setStyleSheet(
        "QPushButton { "
        "  padding: 10px 24px; "
        "  font-size: 10pt; "
        "  font-weight: normal; "
        "  color: #ffffff; "
        "  background: #000000; "
        "  border: none; "
        "  border-radius: 4px; "
        "}"
        "QPushButton:hover { "
        "  background: #333333; "
        "}"
        "QPushButton:pressed { "
        "  background: #666666; "
        "}"
    );
    m_applyBtn->setStyleSheet(m_saveBtn->styleSheet());
    
    buttonLayout->addWidget(m_saveBtn);
    buttonLayout->addWidget(m_cancelBtn);
    buttonLayout->addWidget(m_applyBtn);
    
    mainLayout->addLayout(buttonLayout);
    
    // 连接信号
    connect(m_saveBtn, &QPushButton::clicked, this, &SettingsDialog::onSaveClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &SettingsDialog::onCancelClicked);
    connect(m_applyBtn, &QPushButton::clicked, this, &SettingsDialog::onApplyClicked);
}

void SettingsDialog::setupModelTab()
{
    m_modelTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(m_modelTab);
    
    // ===== 模型列表区域 =====
    QLabel* modelTitle = new QLabel("模型列表");
    modelTitle->setStyleSheet("font-size: 14pt; font-weight: normal; color: #000000;");
    layout->addWidget(modelTitle);
    
    // 模型列表描述
    QLabel* modelDesc = new QLabel("按提供商分类配置模型及其 API Key / API URL");
    modelDesc->setStyleSheet("color: #666666; font-size: 9pt;");
    modelDesc->setWordWrap(true);
    layout->addWidget(modelDesc);
    
    // 创建模型分类选项卡
    m_modelCategoryTabs = new QTabWidget();
    m_modelCategoryTabs->setStyleSheet(
        "QTabWidget::pane { "
        "  border: 1px solid #e0e0e0; "
        "  border-radius: 4px; "
        "  background: #ffffff; "
        "}"
        "QTabBar::tab { "
        "  background: #f5f5f5; "
        "  color: #666666; "
        "  padding: 8px 20px; "
        "  margin-right: 2px; "
        "  border-top-left-radius: 4px; "
        "  border-top-right-radius: 4px; "
        "}"
        "QTabBar::tab:selected { "
        "  background: #ffffff; "
        "  color: #2196F3; "
        "  border-bottom: 2px solid #2196F3; "
        "}"
        "QTabBar::tab:hover { "
        "  background: #eeeeee; "
        "}"
    );
    
    // 选项卡将在 updateModelTabs() 中动态创建
    layout->addWidget(m_modelCategoryTabs);
    
    // 按钮
    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_addModelBtn = new QPushButton("添加模型");
    QIcon addIcon(":/res/08.png");
    m_addModelBtn->setIcon(addIcon);
    m_addModelBtn->setIconSize(QSize(16, 16));
    
    m_editModelBtn = new QPushButton("编辑");
    QIcon editIcon(":/res/09.png");
    m_editModelBtn->setIcon(editIcon);
    m_editModelBtn->setIconSize(QSize(16, 16));
    
    m_removeModelBtn = new QPushButton("删除");
    QIcon removeIcon(":/res/10.png");
    m_removeModelBtn->setIcon(removeIcon);
    m_removeModelBtn->setIconSize(QSize(16, 16));
    
    btnLayout->addWidget(m_addModelBtn);
    btnLayout->addWidget(m_editModelBtn);
    btnLayout->addWidget(m_removeModelBtn);
    btnLayout->addStretch();
    
    layout->addLayout(btnLayout);
    
    // 连接信号
    connect(m_addModelBtn, &QPushButton::clicked, this, &SettingsDialog::onAddModelClicked);
    connect(m_editModelBtn, &QPushButton::clicked, this, &SettingsDialog::onEditModelClicked);
    connect(m_removeModelBtn, &QPushButton::clicked, this, &SettingsDialog::onRemoveModelClicked);
    
    // 选中项变化时更新按钮状态
    auto updateButtonState = [this]() {
        QListWidget* currentList = getCurrentModelList();
        bool hasSelection = currentList && currentList->currentItem() != nullptr;
        m_editModelBtn->setEnabled(hasSelection);
        m_removeModelBtn->setEnabled(hasSelection);
    };
    
    // 切换选项卡时清除选中状态并更新按钮
    connect(m_modelCategoryTabs, &QTabWidget::currentChanged, this, [this, updateButtonState](int) {
        // 清除所有列表的选中状态
        for (QListWidget* list : m_providerModelLists.values()) {
            if (list != getCurrentModelList()) {
                list->clearSelection();
            }
        }
        if (m_localModelList && m_localModelList != getCurrentModelList()) {
            m_localModelList->clearSelection();
        }
        updateButtonState();
    });
    
    // 初始化选项卡
    updateModelTabs();
    
    // 初始状态：禁用编辑和删除按钮
    m_editModelBtn->setEnabled(false);
    m_removeModelBtn->setEnabled(false);
    
    QIcon modelTabIcon(":/res/04.png");
    m_tabWidget->addTab(m_modelTab, modelTabIcon, "模型配置");
}

void SettingsDialog::setupPromptTab()
{
    m_promptTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(m_promptTab);
    
    QLabel* titleLabel = new QLabel("提示词模板");
    titleLabel->setStyleSheet("font-size: 14pt; font-weight: normal; color: #000000;");
    layout->addWidget(titleLabel);
    
    QLabel* descLabel = new QLabel("管理常用的提示词模板，快速切换不同的识别场景");
    descLabel->setStyleSheet("color: #666666; font-size: 9pt;");
    layout->addWidget(descLabel);
    
    // 创建提示词分类选项卡
    m_promptCategoryTabs = new QTabWidget();
    m_promptCategoryTabs->setStyleSheet(
        "QTabWidget::pane { "
        "  border: 1px solid #e0e0e0; "
        "  border-radius: 4px; "
        "  background: #ffffff; "
        "}"
        "QTabBar::tab { "
        "  background: #f5f5f5; "
        "  color: #666666; "
        "  padding: 8px 20px; "
        "  margin-right: 2px; "
        "  border-top-left-radius: 4px; "
        "  border-top-right-radius: 4px; "
        "}"
        "QTabBar::tab:selected { "
        "  background: #ffffff; "
        "  color: #2196F3; "
        "  border-bottom: 2px solid #2196F3; "
        "}"
        "QTabBar::tab:hover { "
        "  background: #eeeeee; "
        "}"
    );
    
    // 选项卡将在 updatePromptTabs() 中动态创建
    layout->addWidget(m_promptCategoryTabs);
    
    // 提示词模板按钮
    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_addPromptBtn = new QPushButton("添加模板");
    QIcon addPromptIcon(":/res/05.png");
    m_addPromptBtn->setIcon(addPromptIcon);
    m_addPromptBtn->setIconSize(QSize(16, 16));
    
    m_editPromptBtn = new QPushButton("编辑");
    QIcon editPromptIcon(":/res/06.png");
    m_editPromptBtn->setIcon(editPromptIcon);
    m_editPromptBtn->setIconSize(QSize(16, 16));
    
    m_removePromptBtn = new QPushButton("删除");
    QIcon removePromptIcon(":/res/07.png");
    m_removePromptBtn->setIcon(removePromptIcon);
    m_removePromptBtn->setIconSize(QSize(16, 16));
    
    btnLayout->addWidget(m_addPromptBtn);
    btnLayout->addWidget(m_editPromptBtn);
    btnLayout->addWidget(m_removePromptBtn);
    btnLayout->addStretch();
    
    layout->addLayout(btnLayout);
    
    // 连接信号
    connect(m_addPromptBtn, &QPushButton::clicked, this, &SettingsDialog::onAddPromptTemplateClicked);
    connect(m_editPromptBtn, &QPushButton::clicked, this, &SettingsDialog::onEditPromptTemplateClicked);
    connect(m_removePromptBtn, &QPushButton::clicked, this, &SettingsDialog::onRemovePromptTemplateClicked);
    
    // 选中项变化时更新按钮状态
    auto updatePromptButtonState = [this]() {
        QListWidget* currentList = getCurrentPromptList();
        bool hasSelection = currentList && currentList->currentItem() != nullptr;
        m_editPromptBtn->setEnabled(hasSelection);
        m_removePromptBtn->setEnabled(hasSelection);
    };
    
    // 切换选项卡时清除选中状态并更新按钮
    connect(m_promptCategoryTabs, &QTabWidget::currentChanged, this, [this, updatePromptButtonState](int) {
        for (QListWidget* list : m_promptTypeLists.values()) {
            if (list != getCurrentPromptList()) {
                list->clearSelection();
            }
        }
        updatePromptButtonState();
    });
    
    // 初始化选项卡
    updatePromptTabs();
    
    // 初始状态：禁用编辑和删除按钮
    m_editPromptBtn->setEnabled(false);
    m_removePromptBtn->setEnabled(false);
    
    QIcon promptTabIcon(":/res/08.png");
    m_tabWidget->addTab(m_promptTab, promptTabIcon, "提示词");
}

void SettingsDialog::setupShortcutTab()
{
    m_shortcutTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(m_shortcutTab);
    
    QLabel* titleLabel = new QLabel("快捷键设置");
    titleLabel->setStyleSheet("font-size: 14pt; font-weight: normal; color: #000000;");
    layout->addWidget(titleLabel);
    
    QLabel* descLabel = new QLabel("自定义全局快捷键，提高使用效率");
    descLabel->setStyleSheet("color: #666666; font-size: 9pt;");
    layout->addWidget(descLabel);
    
    // 快捷键设置
    QFormLayout* formLayout = new QFormLayout();
    
    m_screenshotShortcut = new QKeySequenceEdit();
    formLayout->addRow("截图识别:", m_screenshotShortcut);
    
    m_pasteShortcut = new QKeySequenceEdit();
    formLayout->addRow("粘贴图片:", m_pasteShortcut);
    
    m_recognizeShortcut = new QKeySequenceEdit();
    formLayout->addRow("询问AI:", m_recognizeShortcut);
    
    layout->addLayout(formLayout);
    layout->addStretch();
    
    QIcon shortcutTabIcon(":/res/09.png");
    m_tabWidget->addTab(m_shortcutTab, shortcutTabIcon, "快捷键");
}

void SettingsDialog::setupGeneralTab()
{
    m_generalTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(m_generalTab);
    
    QLabel* titleLabel = new QLabel("通用设置");
    titleLabel->setStyleSheet("font-size: 14pt; font-weight: normal; color: #000000;");
    layout->addWidget(titleLabel);
    
    // 功能选项
    QGroupBox* featureGroup = new QGroupBox("功能选项");
    QVBoxLayout* featureLayout = new QVBoxLayout(featureGroup);
    
    m_autoCopyCheck = new QCheckBox("自动复制识别结果到剪贴板");
    m_autoCopyCheck->setChecked(true);
    featureLayout->addWidget(m_autoCopyCheck);
    
    m_autoSaveCheck = new QCheckBox("自动保存识别结果");
    featureLayout->addWidget(m_autoSaveCheck);
    
    m_autoRecognizeAfterScreenshot = new QCheckBox("截图后自动识别");
    m_autoRecognizeAfterScreenshot->setChecked(true);
    featureLayout->addWidget(m_autoRecognizeAfterScreenshot);
    
    layout->addWidget(featureGroup);
    
    // 历史记录
    QGroupBox* historyGroup = new QGroupBox("历史记录");
    QFormLayout* historyLayout = new QFormLayout(historyGroup);
    
    m_maxHistorySpin = new QSpinBox();
    m_maxHistorySpin->setRange(10, 1000);
    m_maxHistorySpin->setValue(50);
    m_maxHistorySpin->setSuffix(" 条");
    historyLayout->addRow("最大历史记录数:", m_maxHistorySpin);
    
    layout->addWidget(historyGroup);
    
    // 保存位置
    QGroupBox* saveGroup = new QGroupBox("保存设置");
    QHBoxLayout* saveLayout = new QHBoxLayout(saveGroup);
    
    m_saveFolderEdit = new QLineEdit();
    m_saveFolderEdit->setPlaceholderText("选择保存文件夹...");
    QPushButton* browseBtn = new QPushButton("浏览...");
    QIcon browseIcon(":/res/10.png");
    browseBtn->setIcon(browseIcon);
    browseBtn->setIconSize(QSize(16, 16));
    
    saveLayout->addWidget(new QLabel("保存文件夹:"));
    saveLayout->addWidget(m_saveFolderEdit);
    saveLayout->addWidget(browseBtn);
    
    connect(browseBtn, &QPushButton::clicked, this, [this]() {
        QString folder = QFileDialog::getExistingDirectory(this, "选择保存文件夹");
        if (!folder.isEmpty()) {
            m_saveFolderEdit->setText(folder);
        }
    });
    
    layout->addWidget(saveGroup);
    layout->addStretch();
    
    QIcon generalTabIcon(":/res/11.png");
    m_tabWidget->addTab(m_generalTab, generalTabIcon, "通用");
}

/**
 * @brief 设置"关于"标签页的内容和布局
 * 
 * 该函数用于创建"关于"标签页，包含作者信息、软件介绍和版本更新记录等内容
 */
void SettingsDialog::setupAboutTab()
{
    // 创建"关于"标签页的主部件和垂直布局
    m_aboutTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(m_aboutTab);

    // 创建并设置作者信息标签
    QLabel* author = new QLabel("作者：小帅同学, 公众号：小帅随笔");
    author->setStyleSheet("font-size: 11pt; color: #000000;"); 
    layout->addWidget(author);

    QLabel* intro = new QLabel("现代OCR工具，集成视觉语言理解大模型（VLM：Vision-Language Models），开源：https://github.com/xstongxue/XS-VLM-OCR");
    intro->setWordWrap(true);
    intro->setStyleSheet("font-size: 10pt; color: #333333;");
    layout->addWidget(intro);

    QLabel* changelogTitle = new QLabel("版本更新记录");
    changelogTitle->setStyleSheet("font-size: 11pt; font-weight: normal; color: #000000; margin-top: 8px;");
    layout->addWidget(changelogTitle);

    QTextEdit* changelog = new QTextEdit();
    changelog->setReadOnly(true);
    changelog->setStyleSheet(
        "QTextEdit { "
        "  background: #f9f9f9;"
        "  border: 1px solidhsl(0, 0.00%, 87.80%);"
        "  border-radius: 6px;"
        "  padding: 8px;"
        "  font-size: 10pt;"
        "  color: #333333;"
        "}"
    );
    changelog->setText(
        "v1.0.0\n"
        " - 全局快捷键截图（ALT+A）\n"
        " - 主流大模型支持（Qwen、GLM、Paddle、Custom、Tesseract）\n"
        " - 智能提示词管理（识别、翻译、解答、整理四大类）\n"
        " - 现代化UI界面（主题切换、侧边栏折叠）\n"
        " - 历史记录管理\n"
        " - 异步任务处理\n"
        "\n"
        "v1.1.0【待开发】\n"
        " - 批量图片/文件处理\n"
        " - 结果导出多格式（Markdown、PDF、Word、Excel）\n"
        " - 本地大模型支持（支持 Windows 中的 Ollama 部署）\n"
        " - 跨平台支持（Win/Linux/Mac）\n"
        " - 移动端应用\n"
    );
    layout->addWidget(changelog);

    layout->addStretch();

    QIcon aboutTabIcon(":/res/12.png");
    m_tabWidget->addTab(m_aboutTab, aboutTabIcon, "关于");
}

void SettingsDialog::loadSettings()
{
    // 加载所有提供商配置
    QMap<QString, ProviderConfig> providers = m_configManager->getProviders();
    for (auto it = providers.begin(); it != providers.end(); ++it) {
        const QString& providerId = it.key();
        if (m_providerKeyEdits.contains(providerId)) {
            m_providerKeyEdits[providerId]->setText(it.value().apiKey);
        }
        if (m_providerUrlEdits.contains(providerId)) {
            m_providerUrlEdits[providerId]->setText(it.value().apiHost);
        }
    }
    
    // 加载模型列表
    updateModelList();
    
    // 加载提示词模板
    updatePromptList();
    
    // 加载通用设置
    m_autoCopyCheck->setChecked(m_configManager->getSetting("auto_copy_result", true).toBool());
    m_autoSaveCheck->setChecked(m_configManager->getSetting("auto_save", false).toBool());
    m_autoRecognizeAfterScreenshot->setChecked(m_configManager->getSetting("auto_recognize_after_screenshot", false).toBool());
    m_maxHistorySpin->setValue(m_configManager->getSetting("max_history", 50).toInt());
    m_saveFolderEdit->setText(m_configManager->getSetting("save_folder", "./ocr_results").toString());
    
    // 加载快捷键
    m_screenshotShortcut->setKeySequence(QKeySequence(m_configManager->getSetting("shortcut_screenshot", "Ctrl+R").toString()));
    m_pasteShortcut->setKeySequence(QKeySequence(m_configManager->getSetting("shortcut_paste", "Ctrl+V").toString()));
    m_recognizeShortcut->setKeySequence(QKeySequence(m_configManager->getSetting("shortcut_recognize", "Ctrl+E").toString()));
}

void SettingsDialog::saveSettings()
{
    qDebug() << "SettingsDialog: 保存设置...";
    
    // 快捷键校验：不能为空且不能冲突
    QString screenshotKey = m_screenshotShortcut->keySequence().toString().trimmed();
    QString pasteKey = m_pasteShortcut->keySequence().toString().trimmed();
    QString recognizeKey = m_recognizeShortcut->keySequence().toString().trimmed();

    if (screenshotKey.isEmpty() || recognizeKey.isEmpty())
    {
        QMessageBox::warning(this, "快捷键无效", "截图或开始识别的快捷键不能为空，请重新设置。");
        return;
    }
    if (screenshotKey == recognizeKey)
    {
        QMessageBox::warning(this, "快捷键冲突", "截图和开始识别快捷键不能相同，请重新设置。");
        return;
    }
    // 避免与粘贴快捷键冲突
    if (!pasteKey.isEmpty() && (pasteKey == screenshotKey || pasteKey == recognizeKey))
    {
        QMessageBox::warning(this, "快捷键冲突", "粘贴快捷键与其它快捷键重复，请重新设置。");
        return;
    }
    
    // 保存所有提供商配置
    QMap<QString, ProviderConfig> providers = m_configManager->getProviders();
    bool hasProviderChanged = false;
    QStringList updatedProviders;
    
    // 收集所有需要更新的provider ID（包括有Key或URL编辑框的）
    QSet<QString> providerIds;
    for (auto it = m_providerKeyEdits.begin(); it != m_providerKeyEdits.end(); ++it) {
        providerIds.insert(it.key());
    }
    for (auto it = m_providerUrlEdits.begin(); it != m_providerUrlEdits.end(); ++it) {
        providerIds.insert(it.key());
    }
    
    for (const QString& providerId : providerIds) {
        if (providers.contains(providerId)) {
            ProviderConfig provider = providers[providerId];
            bool changed = false;
            
            // 更新 API Key
            if (m_providerKeyEdits.contains(providerId)) {
                QString newApiKey = m_providerKeyEdits[providerId]->text().trimmed();
                if (newApiKey != provider.apiKey) {
                    provider.apiKey = newApiKey;
                    changed = true;
                    qDebug() << QString("  - 更新 %1 API Key: %2")
                        .arg(provider.name)
                        .arg(provider.apiKey.isEmpty() ? "已清空" : "已设置");
                }
            }
            
            // 更新 API URL
            if (m_providerUrlEdits.contains(providerId)) {
                QString newApiUrl = m_providerUrlEdits[providerId]->text().trimmed();
                if (newApiUrl != provider.apiHost) {
                    provider.apiHost = newApiUrl;
                    changed = true;
                    qDebug() << QString("  - 更新 %1 API URL: %2")
                        .arg(provider.name)
                        .arg(provider.apiHost.isEmpty() ? "已清空" : provider.apiHost);
                }
            }
            
            if (changed) {
                m_configManager->updateProvider(providerId, provider);
                hasProviderChanged = true;
                if (!updatedProviders.contains(provider.name)) {
                    updatedProviders << provider.name;
                }
            }
        }
    }
    
    // 保存通用设置
    m_configManager->setSetting("auto_copy_result", m_autoCopyCheck->isChecked());
    m_configManager->setSetting("auto_save", m_autoSaveCheck->isChecked());
    m_configManager->setSetting("auto_recognize_after_screenshot", m_autoRecognizeAfterScreenshot->isChecked());
    m_configManager->setSetting("max_history", m_maxHistorySpin->value());
    m_configManager->setSetting("save_folder", m_saveFolderEdit->text());
    
    // 保存快捷键
    m_configManager->setSetting("shortcut_screenshot", screenshotKey);
    m_configManager->setSetting("shortcut_paste", pasteKey);
    m_configManager->setSetting("shortcut_recognize", recognizeKey);
    
    // 保存到文件
    QString configPath = m_configManager->getConfigPath();
    if (configPath.isEmpty()) {
        // 如果配置路径为空，使用默认路径
        configPath = QDir::currentPath() + "/models_config.json";
        qDebug() << "使用默认配置路径:" << configPath;
    }
    
    bool saved = m_configManager->saveConfig(configPath);
    if (saved) {
        qDebug() << "✓ 配置已保存到:" << configPath;
        m_modified = false;
        emit settingsChanged();
    } else {
        qWarning() << "✗ 保存配置失败";
        QMessageBox::warning(this, "错误", "保存配置失败，请检查文件权限");
    }
}

QListWidget* SettingsDialog::getOrCreateProviderList(const QString& providerId, const QString& providerName)
{
    if (m_providerModelLists.contains(providerId)) {
        return m_providerModelLists[providerId];
    }
    
    // 创建新选项卡
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);  // 配置区域与列表之间留白
    
    // 如果该 provider 存在于配置中，添加 API Key 和 API URL 配置区域
    QMap<QString, ProviderConfig> providers = m_configManager->getProviders();
    if (providers.contains(providerId)) {
        const ProviderConfig& provider = providers[providerId];
        
        // 创建配置区域
        QGroupBox* configGroup = new QGroupBox(QString("%1 配置").arg(provider.name));
        configGroup->setStyleSheet(
            "QGroupBox { "
            "   font-weight: normal; "
            "   font-size: 9pt; "
            "   border: 1px solid #e0e0e0; "
            "   border-radius: 6px; "
            "   margin-top: 8px; "
            "   padding-top: 8px; "
            "   background-color: #fafafa; "
            "   color: #000000; "
            "} "
            "QGroupBox::title { "
            "   subcontrol-origin: margin; "
            "   left: 8px; "
            "   padding: 0 4px; "
            "   color: #666666; "
            "}"
        );
        
        QVBoxLayout* configLayout = new QVBoxLayout(configGroup);
        configLayout->setContentsMargins(8, 8, 8, 8);
        configLayout->setSpacing(8);

        // 描述提示
        if (!provider.description.isEmpty()) {
            QLabel* descLabel = new QLabel(provider.description);
            descLabel->setWordWrap(true);
            descLabel->setStyleSheet("font-size: 8pt; color: #777777;");
            descLabel->setContentsMargins(0, 0, 0, 4);
            configLayout->addWidget(descLabel);
        }
        
        // API Key 输入行
        QHBoxLayout* keyLayout = new QHBoxLayout();
        QLabel* keyLabel = new QLabel("API Key:");
        keyLabel->setStyleSheet("font-size: 9pt;");
        keyLabel->setMinimumWidth(64);
        
        QLineEdit* keyEdit = new QLineEdit();
        keyEdit->setPlaceholderText(QString("请输入 %1 API Key").arg(provider.name));
        keyEdit->setEchoMode(QLineEdit::Password);
        keyEdit->setMinimumHeight(26);
        keyEdit->setStyleSheet("font-size: 9pt;");
        m_providerKeyEdits[providerId] = keyEdit;  // 存储引用
        
        // 加载现有的 API Key
        if (!provider.apiKey.isEmpty()) {
            keyEdit->setText(provider.apiKey);
        }
        
        QPushButton* showBtn = new QPushButton("显示");
        QIcon showIcon(":/res/01.png");
        showBtn->setIcon(showIcon);
        showBtn->setIconSize(QSize(12, 12));
        showBtn->setFixedWidth(72);  // 确保按钮完整可见
        showBtn->setMinimumHeight(26);
        showBtn->setCheckable(true);
        showBtn->setStyleSheet("font-size: 8pt; padding: 2px;");
        
        keyLayout->setSpacing(10);  // 增加布局项之间的间距
        keyLayout->addWidget(keyLabel);
        keyLayout->addWidget(keyEdit, 1);
        keyLayout->addWidget(showBtn);
        configLayout->addLayout(keyLayout);
        
        // API URL 输入行
        QHBoxLayout* urlLayout = new QHBoxLayout();
        QLabel* urlLabel = new QLabel("API URL:");
        urlLabel->setStyleSheet("font-size: 9pt;");
        urlLabel->setMinimumWidth(64);
        
        QLineEdit* urlEdit = new QLineEdit();
        urlEdit->setPlaceholderText(QString("例如: %1").arg(provider.apiHost.isEmpty() ? "https://..." : provider.apiHost));
        urlEdit->setMinimumHeight(26);
        urlEdit->setStyleSheet("font-size: 9pt;");
        m_providerUrlEdits[providerId] = urlEdit;  // 存储引用
        
        // 加载现有的 API URL
        if (!provider.apiHost.isEmpty()) {
            urlEdit->setText(provider.apiHost);
        }
        
        urlLayout->setSpacing(10);  // 增加布局项之间的间距
        urlLayout->addWidget(urlLabel);
        urlLayout->addWidget(urlEdit, 1);
        configLayout->addLayout(urlLayout);
        
        // 显示/隐藏按钮
        connect(showBtn, &QPushButton::toggled, [keyEdit, showBtn](bool checked) {
            keyEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
            showBtn->setText(checked ? "隐藏" : "显示");
        });
        
        // API Key 和 API URL 变更监听（仅标记修改，不显示状态提示）
        connect(keyEdit, &QLineEdit::textChanged, [this]() {
            m_modified = true;
        });
        connect(urlEdit, &QLineEdit::textChanged, [this]() {
            m_modified = true;
        });
        
        layout->addWidget(configGroup);
    }
    
    QString listStyle = 
        "QListWidget { "
        "  border: none; "
        "  background: #ffffff; "
        "  color: #000000; "
        "  padding: 2px; "
        "}"
        "QListWidget::item { "
        "  padding: 8px; "
        "  border-bottom: 1px solid #f0f0f0; "
        "  border-radius: 2px; "
        "}"
        "QListWidget::item:selected { "
        "  background: #e3f2fd; "
        "  color: #000000; "
        "  border-left: 3px solid #2196F3; "
        "}"
        "QListWidget::item:hover { "
        "  background: #f5f5f5; "
        "}"
        "QListWidget::item:selected:hover { "
        "  background: #bbdefb; "
        "}";
    
    QListWidget* list = new QListWidget();
    list->setStyleSheet(listStyle);
    layout->addWidget(list, 1);  // 设置拉伸因子，让列表占据剩余空间
    
    // 直接添加到末尾（离线模型会在最后添加，所以这里直接追加即可）
    m_modelCategoryTabs->addTab(tab, providerName);
    
    // 连接信号
    connect(list, &QListWidget::itemDoubleClicked, this, &SettingsDialog::onEditModelClicked);
    connect(list, &QListWidget::itemSelectionChanged, this, [this]() {
        QListWidget* currentList = getCurrentModelList();
        bool hasSelection = currentList && currentList->currentItem() != nullptr;
        m_editModelBtn->setEnabled(hasSelection);
        m_removeModelBtn->setEnabled(hasSelection);
    });
    
    m_providerModelLists[providerId] = list;
    return list;
}

void SettingsDialog::updateModelTabs()
{
    // 清空所有现有选项卡（除了离线模型）
    while (m_modelCategoryTabs->count() > 0) {
        m_modelCategoryTabs->removeTab(0);
    }
    
    // 清空列表映射
    m_providerModelLists.clear();
    m_localModelList = nullptr;
    
    // 创建列表样式
    QString listStyle = 
        "QListWidget { "
        "  border: none; "
        "  background: #ffffff; "
        "  color: #000000; "
        "  padding: 4px; "
        "}"
        "QListWidget::item { "
        "  padding: 10px; "
        "  border-bottom: 1px solid #f0f0f0; "
        "  border-radius: 2px; "
        "}"
        "QListWidget::item:selected { "
        "  background: #e3f2fd; "
        "  color: #000000; "
        "  border-left: 3px solid #2196F3; "
        "}"
        "QListWidget::item:hover { "
        "  background: #f5f5f5; "
        "}"
        "QListWidget::item:selected:hover { "
        "  background: #bbdefb; "
        "}";
    
    // 获取所有模型和提供商
    QList<ModelConfig> configs = m_configManager->getModelConfigs();
    QMap<QString, ProviderConfig> providers = m_configManager->getProviders();
    
    // 收集所有使用的 provider（排除 local）
    QSet<QString> usedProviders;
    for (const ModelConfig& config : configs) {
        if (config.type != "local" && !config.provider.isEmpty()) {
            usedProviders.insert(config.provider);
        }
    }
    
    // 按 provider 创建选项卡（排除 local、custom这些不需要单独显示的）
    // 优先顺序：阿里云百炼 -> 智谱清言 -> 百度飞桨 -> 通用厂商 -> 其他
    QStringList providerOrder = {"aliyun", "glm", "paddle", "gen"};  // 可以扩展其他提供商
    for (const QString& providerId : providerOrder) {
        if (usedProviders.contains(providerId) && providers.contains(providerId)) {
            QString providerName = providers[providerId].name;
            getOrCreateProviderList(providerId, providerName);
        }
    }
    
    // 处理其他未在 providerOrder 中的 provider
    for (const QString& providerId : usedProviders) {
        if (!providerOrder.contains(providerId) && providers.contains(providerId)) {
            QString providerName = providers[providerId].name;
            getOrCreateProviderList(providerId, providerName);
        }
    }
    
    // 处理 provider 为空但在线的模型（归类到"其他在线模型"）
    bool hasOtherOnline = false;
    for (const ModelConfig& config : configs) {
        if (config.type == "online" && (config.provider.isEmpty() || !providers.contains(config.provider))) {
            hasOtherOnline = true;
            break;
        }
    }
    if (hasOtherOnline) {
        getOrCreateProviderList("other_online", "其他在线模型");
    }
    
    // 处理 custom、gen 等非标准 provider
    for (const ModelConfig& config : configs) {
        if (config.type == "online" && config.provider == "custom") {
            getOrCreateProviderList("custom", "自定义API");
            break;
        }
    }
    
    // 最后一个选项卡：离线模型
    QWidget* localTab = new QWidget();
    QVBoxLayout* localLayout = new QVBoxLayout(localTab);
    localLayout->setContentsMargins(0, 0, 0, 0);
    m_localModelList = new QListWidget();
    m_localModelList->setStyleSheet(listStyle);
    localLayout->addWidget(m_localModelList);
    m_modelCategoryTabs->addTab(localTab, "离线模型");
    
    // 连接离线模型列表的信号
    connect(m_localModelList, &QListWidget::itemDoubleClicked, this, &SettingsDialog::onEditModelClicked);
    connect(m_localModelList, &QListWidget::itemSelectionChanged, this, [this]() {
        QListWidget* currentList = getCurrentModelList();
        bool hasSelection = currentList && currentList->currentItem() != nullptr;
        m_editModelBtn->setEnabled(hasSelection);
        m_removeModelBtn->setEnabled(hasSelection);
    });
}

QListWidget* SettingsDialog::getCurrentModelList() const
{
    int currentIndex = m_modelCategoryTabs->currentIndex();
    if (currentIndex < 0) return nullptr;
    
    QWidget* currentWidget = m_modelCategoryTabs->widget(currentIndex);
    if (!currentWidget) return nullptr;
    
    // 查找当前 widget 中的 QListWidget
    QListWidget* list = currentWidget->findChild<QListWidget*>();
    return list;
}

void SettingsDialog::updateModelList()
{
    // 先更新选项卡结构（确保所有 provider 选项卡都存在）
    updateModelTabs();
    
    // 清空所有列表
    for (QListWidget* list : m_providerModelLists.values()) {
        if (list) list->clear();
    }
    if (m_localModelList) {
        m_localModelList->clear();
    }
    
    QList<ModelConfig> configs = m_configManager->getModelConfigs();
    QMap<QString, ProviderConfig> providers = m_configManager->getProviders();
    
    // 按 provider 分组模型
    for (const ModelConfig& config : configs) {
        QString itemText = config.displayName;
            
            // 添加启用状态
            if (config.enabled) {
                itemText += " - [已启用]";
            } else {
                itemText += " - [已禁用]";
            }
            
            // 在线模型显示 API Key 状态
        if (config.type == "online" && !config.provider.isEmpty() && providers.contains(config.provider)) {
            if (!providers[config.provider].apiKey.isEmpty()) {
                    itemText += " - [已配置]";
                } else {
                    itemText += " - [未配置Key]";
                }
            }
            
            QListWidgetItem* item = new QListWidgetItem(itemText);
            item->setData(Qt::UserRole, config.id);
            
            // 根据状态设置颜色和字体
            QFont itemFont = item->font();
            if (!config.enabled) {
                item->setForeground(QColor(150, 150, 150));
                itemFont.setItalic(true);
            } else {
                itemFont.setItalic(false);
                if (config.type == "local") {
                    item->setForeground(QColor(46, 125, 50)); // 绿色 - 离线模型
                } else {
                    // 在线模型，检查API Key
                if (!config.provider.isEmpty() && providers.contains(config.provider) && !providers[config.provider].apiKey.isEmpty()) {
                        item->setForeground(QColor(25, 118, 210)); // 蓝色 - 已配置
                    } else {
                        item->setForeground(QColor(245, 124, 0)); // 橙色 - 未配置
                    }
                }
            }
            item->setFont(itemFont);
            
        // 根据类型和 provider 添加到对应的列表
        if (config.type == "local") {
            // 离线模型添加到最后一个选项卡
            if (m_localModelList) {
                m_localModelList->addItem(item);
            }
        } 
        else if (config.type == "online") {
            // 在线模型按 provider 分组
            QString providerId = config.provider;
            QString providerName;
            
            if (providerId.isEmpty() || !providers.contains(providerId)) {
                // provider 为空或不存在的，归类到"其他在线模型"
                providerId = "other_online";
                providerName = "其他在线模型";
            } 
            else if (providerId == "custom") 
            {
                providerId = "custom";
                providerName = "自定义API";
            } 
            else // 使用 provider 的显示名称
            {
                providerName = providers[providerId].name;
            }
            
            QListWidget* targetList = m_providerModelLists.value(providerId);
            if (!targetList) {
                // 如果列表不存在，创建它
                targetList = getOrCreateProviderList(providerId, providerName);
            }
            
            if (targetList) {
                targetList->addItem(item);
            }
        }
    }
}

QListWidget* SettingsDialog::getOrCreatePromptTypeList(const QString& type, const QString& typeName)
{
    if (m_promptTypeLists.contains(type)) {
        return m_promptTypeLists[type];
    }
    
    // 创建新选项卡
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(0, 0, 0, 0);
    
    QString listStyle = 
        "QListWidget { "
        "  border: none; "
        "  background: #ffffff; "
        "  color: #000000; "
        "  padding: 4px; "
        "}"
        "QListWidget::item { "
        "  padding: 12px; "
        "  border-bottom: 1px solid #f0f0f0; "
        "  border-radius: 2px; "
        "  min-height: 50px; "
        "}"
        "QListWidget::item:selected { "
        "  background: #e3f2fd; "
        "  color: #000000; "
        "  border-left: 3px solid #2196F3; "
        "}"
        "QListWidget::item:hover { "
        "  background: #f5f5f5; "
        "}"
        "QListWidget::item:selected:hover { "
        "  background: #bbdefb; "
        "}";
    
    QListWidget* list = new QListWidget();
    list->setStyleSheet(listStyle);
    layout->addWidget(list);
    
    m_promptCategoryTabs->addTab(tab, typeName);
    
    // 连接信号
    connect(list, &QListWidget::itemDoubleClicked, this, &SettingsDialog::onEditPromptTemplateClicked);
    connect(list, &QListWidget::itemSelectionChanged, this, [this]() {
        QListWidget* currentList = getCurrentPromptList();
        bool hasSelection = currentList && currentList->currentItem() != nullptr;
        m_editPromptBtn->setEnabled(hasSelection);
        m_removePromptBtn->setEnabled(hasSelection);
    });
    
    m_promptTypeLists[type] = list;
    return list;
}

void SettingsDialog::updatePromptTabs()
{
    // 清空所有现有选项卡
    while (m_promptCategoryTabs->count() > 0) {
        m_promptCategoryTabs->removeTab(0);
    }
    
    // 清空列表映射
    m_promptTypeLists.clear();
    
    // 获取所有模板，按类型分组
    QList<PromptTemplate> templates = m_configManager->getPromptTemplates();
    QMap<QString, QList<PromptTemplate>> typeGroups;
    for (const PromptTemplate& tmpl : templates) {
        typeGroups[tmpl.type].append(tmpl);
    }
    
    // 定义类型顺序
    QStringList typeOrder = {"识别", "翻译", "解答", "整理"};
    
    // 按类型顺序创建选项卡
    for (const QString& type : typeOrder) {
        if (typeGroups.contains(type)) {
            getOrCreatePromptTypeList(type, type);
        }
    }
    
    // 处理其他未在typeOrder中的类型
    for (auto it = typeGroups.begin(); it != typeGroups.end(); ++it) {
        if (!typeOrder.contains(it.key())) {
            getOrCreatePromptTypeList(it.key(), it.key());
        }
    }
}

QListWidget* SettingsDialog::getCurrentPromptList() const
{
    int currentIndex = m_promptCategoryTabs->currentIndex();
    if (currentIndex < 0) return nullptr;
    
    QWidget* currentWidget = m_promptCategoryTabs->widget(currentIndex);
    if (!currentWidget) return nullptr;
    
    // 查找当前 widget 中的 QListWidget
    QListWidget* list = currentWidget->findChild<QListWidget*>();
    return list;
}

void SettingsDialog::updatePromptList()
{
    // 先更新选项卡结构（确保所有类型选项卡都存在）
    updatePromptTabs();
    
    // 清空所有列表
    for (QListWidget* list : m_promptTypeLists.values()) {
        if (list) list->clear();
    }
    
    // 从配置加载提示词模板，按类型分组
    QList<PromptTemplate> templates = m_configManager->getPromptTemplates();
    
    // 按类型分组
    QMap<QString, QList<PromptTemplate>> typeGroups;
    for (const PromptTemplate& tmpl : templates) {
        typeGroups[tmpl.type].append(tmpl);
    }
    
    // 填充每个类型的列表
    for (auto it = typeGroups.begin(); it != typeGroups.end(); ++it) {
        const QString& type = it.key();
        QListWidget* typeList = m_promptTypeLists.value(type);
        if (!typeList) {
            typeList = getOrCreatePromptTypeList(type, type);
        }
        
        // 添加该类型下的所有模板
        for (const PromptTemplate& tmpl : it.value()) {
        QString displayText;
        if (tmpl.content.length() > 100) {
                displayText = QString("%1 [%2]\n    %3...").arg(tmpl.name, tmpl.category, tmpl.content.left(100));
        } else {
                displayText = QString("%1 [%2]\n    %3").arg(tmpl.name, tmpl.category, tmpl.content);
        }
        
        QListWidgetItem* item = new QListWidgetItem(displayText);
            item->setData(Qt::UserRole, tmpl.type);  // 存储类型
        item->setData(Qt::UserRole + 1, tmpl.content);
        item->setData(Qt::UserRole + 2, tmpl.name);
            item->setData(Qt::UserRole + 3, tmpl.category);
        
        QFont itemFont = item->font();
        itemFont.setPointSize(10);
            itemFont.setBold(false);
        item->setFont(itemFont);
        item->setSizeHint(QSize(item->sizeHint().width(), 65));
        
            typeList->addItem(item);
        }
    }
}

void SettingsDialog::onSaveClicked()
{
    saveSettings();
    accept();
}

void SettingsDialog::onCancelClicked()
{
    reject();
}

void SettingsDialog::onApplyClicked()
{
    saveSettings();
}

void SettingsDialog::onAddModelClicked()
{
    ModelEditDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        ModelConfig config = dialog.getModelConfig();
        m_configManager->updateModelConfig(config.id, config);
        updateModelList();
        m_modified = true;
        
        // 自动选中新添加的模型（切换到对应的选项卡并选中）
        QListWidget* targetList = nullptr;
        int targetIndex = -1;
        
        if (config.type == "local") {
            // 离线模型在最后一个选项卡
            targetIndex = m_modelCategoryTabs->count() - 1;
            targetList = m_localModelList;
        } else if (config.type == "online") {
            // 找到对应的 provider 选项卡
            QString providerId = config.provider.isEmpty() ? "other_online" : config.provider;
            if (m_providerModelLists.contains(providerId)) {
                targetList = m_providerModelLists[providerId];
                // 找到选项卡索引
                for (int i = 0; i < m_modelCategoryTabs->count() - 1; i++) {
                    QWidget* widget = m_modelCategoryTabs->widget(i);
                    if (widget && widget->findChild<QListWidget*>() == targetList) {
                        targetIndex = i;
                        break;
                    }
                }
            }
        }
        
        if (targetIndex >= 0) {
            m_modelCategoryTabs->setCurrentIndex(targetIndex);
        }
        
        if (targetList) {
            for (int i = 0; i < targetList->count(); ++i) {
                QListWidgetItem* item = targetList->item(i);
            if (item && item->data(Qt::UserRole).toString() == config.id) {
                    targetList->setCurrentItem(item);
                    targetList->scrollToItem(item);
                break;
                }
            }
        }
        
        qDebug() << "添加模型:" << config.id;
    }
}

void SettingsDialog::onEditModelClicked()
{
    QListWidget* currentList = getCurrentModelList();
    if (!currentList) {
        QMessageBox::information(this, "提示", "请先选择一个模型");
        return;
    }
    
    QListWidgetItem* item = currentList->currentItem();
    if (!item) {
        QMessageBox::information(this, "提示", "请先选择一个模型");
        return;
    }
    
    QString modelId = item->data(Qt::UserRole).toString();
    QList<ModelConfig> configs = m_configManager->getModelConfigs();
    
    for (const ModelConfig& config : configs) {
        if (config.id == modelId) {
            ModelEditDialog dialog(config, this);
            if (dialog.exec() == QDialog::Accepted) {
                ModelConfig newConfig = dialog.getModelConfig();
                m_configManager->updateModelConfig(modelId, newConfig);
                updateModelList();
                m_modified = true;
                
                // 重新选中编辑后的模型（切换到对应的选项卡）
                QListWidget* targetList = nullptr;
                int targetIndex = -1;
                
                if (newConfig.type == "local") {
                    // 离线模型在最后一个选项卡
                    targetIndex = m_modelCategoryTabs->count() - 1;
                    targetList = m_localModelList;
                } else if (newConfig.type == "online") {
                    // 找到对应的 provider 选项卡
                    QString providerId = newConfig.provider.isEmpty() ? "other_online" : newConfig.provider;
                    if (m_providerModelLists.contains(providerId)) {
                        targetList = m_providerModelLists[providerId];
                        // 找到选项卡索引
                        for (int i = 0; i < m_modelCategoryTabs->count() - 1; i++) {
                            QWidget* widget = m_modelCategoryTabs->widget(i);
                            if (widget && widget->findChild<QListWidget*>() == targetList) {
                                targetIndex = i;
                                break;
                            }
                        }
                    }
                }
                
                if (targetIndex >= 0) {
                    m_modelCategoryTabs->setCurrentIndex(targetIndex);
                }
                
                if (targetList) {
                    for (int i = 0; i < targetList->count(); ++i) {
                        QListWidgetItem* newItem = targetList->item(i);
                    if (newItem && newItem->data(Qt::UserRole).toString() == modelId) {
                            targetList->setCurrentItem(newItem);
                            targetList->scrollToItem(newItem);
                        break;
                        }
                    }
                }
                
                qDebug() << "编辑模型:" << modelId;
            }
            break;
        }
    }
}

void SettingsDialog::onRemoveModelClicked()
{
    QListWidget* currentList = getCurrentModelList();
    if (!currentList) {
        QMessageBox::information(this, "提示", "请先选择一个模型");
        return;
    }
    
    QListWidgetItem* item = currentList->currentItem();
    if (!item) {
        QMessageBox::information(this, "提示", "请先选择一个模型");
        return;
    }
    
    QString modelId = item->data(Qt::UserRole).toString();
    
    // 获取模型名称用于确认提示
    QList<ModelConfig> configs = m_configManager->getModelConfigs();
    QString modelName = modelId;
    for (const ModelConfig& config : configs) {
        if (config.id == modelId) {
            modelName = config.displayName;
            break;
        }
    }
    
    auto reply = QMessageBox::question(this, "确认删除", 
        QString("确定要删除模型配置吗？\n\n模型名称: %1\n\n此操作不可撤销。").arg(modelName),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        // 从配置管理器中删除
        m_configManager->removeModelConfig(modelId);
        updateModelList();  // 重新加载列表以保持一致性
        m_modified = true;
        
        qDebug() << "删除模型:" << modelId;
    }
}

void SettingsDialog::onAddPromptTemplateClicked()
{
    PromptTemplateDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QString name = dialog.getName();
        QString content = dialog.getContent();
        QString type = dialog.getType();
        QString category = dialog.getCategory();
        
        PromptTemplate tmpl(name, content, type, category);
        m_configManager->addPromptTemplate(tmpl);
        updatePromptList();  // 重新加载列表以使用新的显示格式
        m_modified = true;
        
        // 自动选中新添加的模板（切换到对应的选项卡）
        QString templateType = type;  // 保存类型以便查找
        QListWidget* targetList = m_promptTypeLists.value(templateType);
        if (!targetList && !templateType.isEmpty()) {
            targetList = getOrCreatePromptTypeList(templateType, templateType);
        }
        
        // 切换到对应的选项卡
        if (targetList) {
            for (int i = 0; i < m_promptCategoryTabs->count(); i++) {
                QWidget* widget = m_promptCategoryTabs->widget(i);
                if (widget && widget->findChild<QListWidget*>() == targetList) {
                    m_promptCategoryTabs->setCurrentIndex(i);
                    break;
                }
            }
            
            // 选中新添加的项
            for (int i = 0; i < targetList->count(); ++i) {
                QListWidgetItem* item = targetList->item(i);
            if (item && item->data(Qt::UserRole + 2).toString() == name) {
                    targetList->setCurrentItem(item);
                    targetList->scrollToItem(item);
                break;
                }
            }
        }
        
        qDebug() << "添加提示词模板:" << name << "分类:" << category;
    }
}

void SettingsDialog::onEditPromptTemplateClicked()
{
    QListWidget* currentList = getCurrentPromptList();
    if (!currentList) {
        QMessageBox::information(this, "提示", "请先选择一个模板");
        return;
    }
    
    QListWidgetItem* item = currentList->currentItem();
    if (!item) {
        QMessageBox::information(this, "提示", "请先选择一个模板");
        return;
    }
    
    // 获取模板信息
    QString templateName = item->data(Qt::UserRole + 2).toString();
    QString templateType = item->data(Qt::UserRole).toString();
    
    // 从配置中找到对应的模板索引
    QList<PromptTemplate> templates = m_configManager->getPromptTemplates();
    int templateIndex = -1;
    for (int i = 0; i < templates.size(); i++) {
        if (templates[i].name == templateName) {
            templateIndex = i;
            break;
        }
    }
    
    if (templateIndex >= 0) {
        PromptTemplate tmpl = templates[templateIndex];
        PromptTemplateDialog dialog(tmpl.name, tmpl.content, tmpl.type, tmpl.category, this);
        if (dialog.exec() == QDialog::Accepted) {
            tmpl.name = dialog.getName();
            tmpl.content = dialog.getContent();
            tmpl.type = dialog.getType();
            tmpl.category = dialog.getCategory();
            
            m_configManager->updatePromptTemplate(templateIndex, tmpl);
            updatePromptList();  // 重新加载列表
            m_modified = true;
            
            // 重新选中编辑后的模板（切换到对应的选项卡）
            QString newType = tmpl.type;
            QListWidget* targetList = m_promptTypeLists.value(newType);
            if (targetList) {
                // 切换到对应的选项卡
                for (int i = 0; i < m_promptCategoryTabs->count(); i++) {
                    QWidget* widget = m_promptCategoryTabs->widget(i);
                    if (widget && widget->findChild<QListWidget*>() == targetList) {
                        m_promptCategoryTabs->setCurrentIndex(i);
                        break;
                    }
                }
                
                // 选中编辑后的项
                for (int i = 0; i < targetList->count(); ++i) {
                    QListWidgetItem* newItem = targetList->item(i);
                if (newItem && newItem->data(Qt::UserRole + 2).toString() == tmpl.name) {
                        targetList->setCurrentItem(newItem);
                        targetList->scrollToItem(newItem);
                    break;
                    }
                }
            }
            
            qDebug() << "编辑提示词模板:" << tmpl.name;
        }
    }
}

void SettingsDialog::onRemovePromptTemplateClicked()
{
    QListWidget* currentList = getCurrentPromptList();
    if (!currentList) {
        QMessageBox::information(this, "提示", "请先选择一个模板");
        return;
    }
    
    QListWidgetItem* item = currentList->currentItem();
    if (!item) {
        QMessageBox::information(this, "提示", "请先选择一个模板");
        return;
    }
    
    // 获取模板名称
    QString templateName = item->data(Qt::UserRole + 2).toString();
    
    // 从配置中找到对应的模板索引
    QList<PromptTemplate> templates = m_configManager->getPromptTemplates();
    int templateIndex = -1;
    for (int i = 0; i < templates.size(); i++) {
        if (templates[i].name == templateName) {
            templateIndex = i;
            break;
        }
    }
    
    if (templateIndex >= 0) {
        auto reply = QMessageBox::question(this, "确认", QString("确定要删除模板 \"%1\" 吗？").arg(templateName));
    if (reply == QMessageBox::Yes) {
            m_configManager->removePromptTemplate(templateIndex);
            updatePromptList();  // 重新加载列表
        m_modified = true;
            qDebug() << "删除提示词模板:" << templateName;
        }
    }
}

void SettingsDialog::onTestApiKeyClicked()
{
    QString apiKey = m_providerKeyEdits.value("aliyun")->text().trimmed();
    
    if (apiKey.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先输入 API Key");
        return;
    }
    
    // 简单的格式验证
    if (!apiKey.startsWith("sk-")) {
        QMessageBox::warning(this, "格式错误", 
            "阿里云 API Key 格式不正确\n"
            "应该以 'sk-' 开头");
        return;
    }
    
    QMessageBox::information(this, "API Key 验证", 
        "API Key 格式正确！\n\n"
        "提示：完整的连接测试需要实际调用 API，\n"
        "建议保存设置后尝试识别图片来验证。\n\n"
        "点击'保存'或'应用'后，所有阿里云模型将自动使用此 API Key。");
}

// ==================== ModelEditDialog ====================
ModelEditDialog::ModelEditDialog(const ModelConfig& config, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("编辑模型");
    setupUI();
    loadConfig(config);
}

ModelEditDialog::ModelEditDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("添加模型");
    setupUI();
    // 添加新模型时，默认选中 custom 引擎
    int customIndex = m_engineCombo->findData("custom");
    if (customIndex >= 0) {
        m_engineCombo->setCurrentIndex(customIndex);
    }
}

void ModelEditDialog::setupUI()
{
    resize(550, 650);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // 基本信息
    QFormLayout* formLayout = new QFormLayout();
    
    m_idEdit = new QLineEdit();
    m_idEdit->setPlaceholderText("模型唯一标识，如: my_qwen_model");
    formLayout->addRow("模型 ID*:", m_idEdit);
    
    m_nameEdit = new QLineEdit();
    m_nameEdit->setPlaceholderText("显示名称，如: 我的 Qwen 模型");
    formLayout->addRow("显示名称*:", m_nameEdit);
    
    m_typeCombo = new QComboBox();
    m_typeCombo->addItem("local (离线)", "local");
    m_typeCombo->addItem("online (在线)", "online");
    formLayout->addRow("类型*:", m_typeCombo);
    
    m_engineCombo = new QComboBox();
    m_engineCombo->addItem("tesseract (Tesseract OCR)", "tesseract");
    m_engineCombo->addItem("qwen (阿里通义千问)", "qwen");
    m_engineCombo->addItem("paddle (百度PaddleOCR)", "paddle");
    m_engineCombo->addItem("custom (自定义OpenAI格式API)", "custom");
    m_engineCombo->addItem("glm (智谱清言)", "glm");
    formLayout->addRow("引擎*:", m_engineCombo);
    
    // Provider 选择
    m_providerCombo = new QComboBox();
    m_providerCombo->addItem("不使用提供商（手动配置）", "");
    m_providerCombo->addItem("阿里云百炼 (通义千问)", "aliyun");
    m_providerCombo->addItem("智谱清言 (GLM)", "glm");
    m_providerCombo->addItem("百度飞桨 (PaddleOCR)", "paddle");
    m_providerCombo->addItem("通用厂商 (OpenAI兼容接口)", "gen");
    formLayout->addRow("API 提供商:", m_providerCombo);
    
    m_enabledCheck = new QCheckBox("启用此模型*:");
    m_enabledCheck->setChecked(true);
    formLayout->addRow("", m_enabledCheck);
    
    mainLayout->addLayout(formLayout);
    
    // 引擎参数
    QGroupBox* paramsGroup = new QGroupBox("引擎参数");
    QFormLayout* paramsLayout = new QFormLayout(paramsGroup);
    
    // API Key 提示标签
    m_apiKeyLabel = new QLabel();
    m_apiKeyLabel->setWordWrap(true);
    m_apiKeyLabel->setStyleSheet("color: #2196F3; background-color: #E3F2FD; padding: 8px; border-radius: 4px;");
    m_apiKeyLabel->setText("💡 提示：选择了提供商后，将使用提供商统一配置的 API Key");
    m_apiKeyLabel->setVisible(false);
    paramsLayout->addRow("", m_apiKeyLabel);
    
    // API Key
    m_apiKeyEdit = new QLineEdit();
    m_apiKeyEdit->setPlaceholderText("API Key (在线模型需要)");
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    
    QHBoxLayout* apiKeyLayout = new QHBoxLayout();
    apiKeyLayout->addWidget(m_apiKeyEdit);
    m_testApiBtn = new QPushButton("测试");
    QIcon testIcon(":/res/11.png");
    m_testApiBtn->setIcon(testIcon);
    m_testApiBtn->setIconSize(QSize(16, 16));
    apiKeyLayout->addWidget(m_testApiBtn);
    paramsLayout->addRow("API Key:", apiKeyLayout);
    
    // API URL
    m_apiUrlEdit = new QLineEdit();
    m_apiUrlEdit->setPlaceholderText("例如：https://openai.com 或 http://192.168.4.22:9008");
    paramsLayout->addRow("API URL:", m_apiUrlEdit);
    
    m_modelNameEdit = new QLineEdit();
    m_modelNameEdit->setPlaceholderText("qwen-vl-plus");
    paramsLayout->addRow("模型名称:", m_modelNameEdit);
    
    m_langEdit = new QLineEdit();
    m_langEdit->setPlaceholderText("chi_sim+eng (Tesseract 用)");
    paramsLayout->addRow("语言:", m_langEdit);
    
    m_pathEdit = new QLineEdit();
    m_pathEdit->setPlaceholderText("C:/Program Files/Tesseract-OCR/tesseract.exe");
    QPushButton* browseBtn = new QPushButton("浏览...");
    QIcon browsePathIcon(":/res/12.png");
    browseBtn->setIcon(browsePathIcon);
    browseBtn->setIconSize(QSize(16, 16));
    QHBoxLayout* pathLayout = new QHBoxLayout();
    pathLayout->addWidget(m_pathEdit);
    pathLayout->addWidget(browseBtn);
    paramsLayout->addRow("可执行文件路径:", pathLayout);
    
    m_temperatureEdit = new QDoubleSpinBox();
    m_temperatureEdit->setRange(0.0, 1.0);
    m_temperatureEdit->setSingleStep(0.1);
    m_temperatureEdit->setDecimals(2);
    m_temperatureEdit->setValue(0.1);
    m_temperatureEdit->setToolTip("控制模型输出的随机性和创造性。\n"
                                   "较低值（0.0-0.3）：输出更确定、更准确，适合OCR识别\n"
                                   "较高值（0.7-1.0）：输出更随机、更有创造性\n"
                                   "推荐OCR使用：0.1-0.3");
    QLabel* temperatureLabel = new QLabel("温度:");
    temperatureLabel->setToolTip(m_temperatureEdit->toolTip());
    paramsLayout->addRow(temperatureLabel, m_temperatureEdit);
    
    // 思考模式
    m_enableThinkingCheck = new QCheckBox("启用思考模式 (enable_thinking)");
    m_enableThinkingCheck->setToolTip("启用后，模型会显示推理过程（仅支持思考模式的模型）");
    m_enableThinkingCheck->setChecked(false);  // 默认关闭
    paramsLayout->addRow("", m_enableThinkingCheck);
    
    mainLayout->addWidget(paramsGroup);
    
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    m_okBtn = new QPushButton("确定");
    QIcon okIcon(":/res/05.png");
    m_okBtn->setIcon(okIcon);
    m_okBtn->setIconSize(QSize(16, 16));
    
    m_cancelBtn = new QPushButton("取消");
    QIcon cancelDialogIcon(":/res/06.png");
    m_cancelBtn->setIcon(cancelDialogIcon);
    m_cancelBtn->setIconSize(QSize(16, 16));
    btnLayout->addWidget(m_okBtn);
    btnLayout->addWidget(m_cancelBtn);
    
    mainLayout->addLayout(btnLayout);
    
    // 连接信号
    connect(m_okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_testApiBtn, &QPushButton::clicked, this, &ModelEditDialog::onTestApiKey);
    connect(browseBtn, &QPushButton::clicked, this, [this]() {
        QString file = QFileDialog::getOpenFileName(this, "选择可执行文件");
        if (!file.isEmpty()) {
            m_pathEdit->setText(file);
        }
    });
    
    // Provider 选择改变时的处理
    connect(m_providerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        QString providerId = m_providerCombo->currentData().toString();
        bool useProvider = !providerId.isEmpty();
        QString modelType = m_typeCombo->currentData().toString();
        
        // 对于离线模型（local），即使使用provider，API URL也应该可用（如本地部署的Qwen模型）
        bool isLocalModel = (modelType == "local");
        
        // 如果使用 provider 且不是离线模型，禁用并清空 API Key 和 URL 字段
        if (useProvider && !isLocalModel) {
            m_apiKeyEdit->setEnabled(false);
            m_apiUrlEdit->setEnabled(false);
            m_apiKeyLabel->setVisible(true);
            m_apiKeyEdit->setPlaceholderText("使用提供商统一配置");
            m_apiUrlEdit->setPlaceholderText("使用提供商统一配置");
        } 
        else // 离线模型或未使用provider时，启用字段
        {
            m_apiKeyEdit->setEnabled(true);
            m_apiUrlEdit->setEnabled(true);
            m_apiKeyLabel->setVisible(false);
            if (isLocalModel) {
                m_apiKeyEdit->setPlaceholderText("API Key (本地模型可选)");
                m_apiUrlEdit->setPlaceholderText("例如: http://192.168.4.22:9008");
            } else {
                m_apiKeyEdit->setPlaceholderText("API Key (在线模型需要)");
                m_apiUrlEdit->setPlaceholderText("例如: https://openai.com");
            }
        }
    });
    
    // 类型改变时也需要更新字段状态
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        // 触发provider选择改变的处理，以更新字段状态
        m_providerCombo->currentIndexChanged(m_providerCombo->currentIndex());
    });
}

void ModelEditDialog::loadConfig(const ModelConfig& config)
{
    m_idEdit->setText(config.id);
    m_nameEdit->setText(config.displayName);
    
    // 使用 findData 来根据实际值查找并设置选项
    int typeIndex = m_typeCombo->findData(config.type);
    if (typeIndex >= 0) {
        m_typeCombo->setCurrentIndex(typeIndex);
    }
    
    int engineIndex = m_engineCombo->findData(config.engine);
    if (engineIndex >= 0) {
        m_engineCombo->setCurrentIndex(engineIndex);
    }
    
    m_enabledCheck->setChecked(config.enabled);
    
    // 设置 provider
    QString provider = config.provider;
    for (int i = 0; i < m_providerCombo->count(); ++i) {
        if (m_providerCombo->itemData(i).toString() == provider) {
            m_providerCombo->setCurrentIndex(i);
            break;
        }
    }
    
    // 加载 API Key 和 API URL
    // 对于离线模型（local），即使有provider也应该显示API URL（如本地部署的Qwen模型）
    // 对于在线模型，如果不使用provider则显示，使用provider则使用统一配置
    if (provider.isEmpty() || config.type == "local") {
        m_apiKeyEdit->setText(config.params.value("api_key", ""));
        // 优先使用 api_url（PaddleOCR 使用），如果没有则使用 api_host 或 llm_host（其他引擎使用）
        QString apiUrl = config.params.value("api_url", "");
        if (apiUrl.isEmpty()) {
            apiUrl = config.params.value("api_host", "");
        }
        if (apiUrl.isEmpty()) {
            apiUrl = config.params.value("llm_host", "");
        }
        m_apiUrlEdit->setText(apiUrl);
    } else {
        // 使用provider时，清空字段但保留可见（用户可以查看）
        m_apiKeyEdit->setText("");
        m_apiUrlEdit->setText("");
    }
    
    m_modelNameEdit->setText(config.params.value("model_name", ""));
    m_langEdit->setText(config.params.value("lang", ""));
    m_pathEdit->setText(config.params.value("path", ""));
    QString tempStr = config.params.value("temperature", "0.1");
    bool ok;
    double tempValue = tempStr.toDouble(&ok);
    if (ok && tempValue >= 0.0 && tempValue <= 1.0) {
        m_temperatureEdit->setValue(tempValue);
    } else {
        m_temperatureEdit->setValue(0.1);
    }
    
    // 加载思考模式参数（默认 false）
    QString thinkingStr = config.params.value("enable_thinking", "false").toLower();
    bool enableThinking = (thinkingStr == "true" || thinkingStr == "1");
    m_enableThinkingCheck->setChecked(enableThinking);
    
    // 加载配置后，触发一次provider选择改变的处理，以更新字段状态（特别是离线模型的API URL字段）
    // 使用 QTimer::singleShot 确保在UI完全加载后再触发
    QMetaObject::invokeMethod(this, "onProviderComboChanged", Qt::QueuedConnection);
}

ModelConfig ModelEditDialog::getModelConfig() const
{
    ModelConfig config;
    config.id = m_idEdit->text();
    config.displayName = m_nameEdit->text();
    // 使用 currentData() 获取实际值（不包含说明文字）
    config.type = m_typeCombo->currentData().toString();
    config.engine = m_engineCombo->currentData().toString();
    config.enabled = m_enabledCheck->isChecked();
    config.provider = m_providerCombo->currentData().toString();
    
    // 只有在不使用 provider 时才保存这些字段
    if (config.provider.isEmpty()) {
        if (!m_apiKeyEdit->text().isEmpty())
            config.params["api_key"] = m_apiKeyEdit->text();
        if (!m_apiUrlEdit->text().isEmpty()) {
            config.params["api_host"] = m_apiUrlEdit->text();
        }
    }
    
    if (!m_modelNameEdit->text().isEmpty())
        config.params["model_name"] = m_modelNameEdit->text();
    if (!m_langEdit->text().isEmpty())
        config.params["lang"] = m_langEdit->text();
    if (!m_pathEdit->text().isEmpty())
        config.params["path"] = m_pathEdit->text();
    config.params["temperature"] = QString::number(m_temperatureEdit->value(), 'f', 2);
    
    config.params["enable_thinking"] = m_enableThinkingCheck->isChecked() ? "true" : "false";
    
    config.params["deploy_type"] = config.type == "local" ? "local" : "online";
    
    return config;
}

void ModelEditDialog::onTypeChanged(int index)
{
    // TODO: 根据类型显示/隐藏相关字段
}

void ModelEditDialog::onTestApiKey()
{
    // TODO: 实现 API Key 测试
    QMessageBox::information(this, "提示", "测试功能开发中...");
}

PromptTemplateDialog::PromptTemplateDialog(const QString& name, const QString& content, const QString& type, const QString& category, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("编辑提示词模板");
    setupUI();
    m_nameEdit->setText(name);
    m_contentEdit->setPlainText(content);
    int typeIndex = m_typeCombo->findData(type);
    if (typeIndex >= 0) {
        m_typeCombo->setCurrentIndex(typeIndex);
    }
    m_categoryCombo->setCurrentText(category);
}

PromptTemplateDialog::PromptTemplateDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("新建提示词模板");
    setupUI();
}

void PromptTemplateDialog::setupUI()
{
    resize(500, 400);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    QFormLayout* formLayout = new QFormLayout();
    
    m_nameEdit = new QLineEdit();
    m_nameEdit->setPlaceholderText("模板名称，如: 表格识别");
    formLayout->addRow("名称*:", m_nameEdit);
    
    // 主分类（识别、翻译、解答、整理）
    m_typeCombo = new QComboBox();
    m_typeCombo->addItem("识别", "识别");
    m_typeCombo->addItem("翻译", "翻译");
    m_typeCombo->addItem("解答", "解答");
    m_typeCombo->addItem("整理", "整理");
    formLayout->addRow("类型*:", m_typeCombo);
    
    // 子分类
    m_categoryCombo = new QComboBox();
    m_categoryCombo->addItems({"通用", "表格", "公式", "手写", "证件", "票据", "代码", "截图", "文档", "生活", "结构化", "其他"});
    formLayout->addRow("分类:", m_categoryCombo);
    
    mainLayout->addLayout(formLayout);
    
    QLabel* contentLabel = new QLabel("内容*:");
    mainLayout->addWidget(contentLabel);
    
    m_contentEdit = new QTextEdit();
    m_contentEdit->setPlaceholderText("输入提示词内容，如: 请识别图片中的表格...");
    mainLayout->addWidget(m_contentEdit);
    
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    m_okBtn = new QPushButton("确定");
    QIcon okIcon(":/res/05.png");
    m_okBtn->setIcon(okIcon);
    m_okBtn->setIconSize(QSize(16, 16));
    
    m_cancelBtn = new QPushButton("取消");
    QIcon cancelDialogIcon(":/res/06.png");
    m_cancelBtn->setIcon(cancelDialogIcon);
    m_cancelBtn->setIconSize(QSize(16, 16));
    btnLayout->addWidget(m_okBtn);
    btnLayout->addWidget(m_cancelBtn);
    
    mainLayout->addLayout(btnLayout);
    
    connect(m_okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

QString PromptTemplateDialog::getName() const
{
    return m_nameEdit->text();
}

QString PromptTemplateDialog::getContent() const
{
    return m_contentEdit->toPlainText();
}

QString PromptTemplateDialog::getType() const
{
    return m_typeCombo->currentData().toString();
}

QString PromptTemplateDialog::getCategory() const
{
    return m_categoryCombo->currentText();
}
