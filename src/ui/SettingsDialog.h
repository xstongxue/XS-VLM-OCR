#pragma once
#include <QDialog>
#include <QTabWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QListWidget>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QKeySequenceEdit>
#include "../utils/ConfigManager.h"
#include <QLabel>

// 设置对话框
class SettingsDialog : public QDialog {
    Q_OBJECT
    
public:
    explicit SettingsDialog(ConfigManager* configManager, QWidget* parent = nullptr);
    ~SettingsDialog() override = default;
    
signals:
    void settingsChanged();
    
private slots:
    void onSaveClicked();
    void onCancelClicked();
    void onApplyClicked();
    void onAddModelClicked();
    void onEditModelClicked();
    void onRemoveModelClicked();
    void onAddPromptTemplateClicked();
    void onEditPromptTemplateClicked();
    void onRemovePromptTemplateClicked();
    void onTestApiKeyClicked();
    
private:
    void setupUI();
    void setupModelTab();
    void setupPromptTab();
    void setupShortcutTab();
    void setupGeneralTab();
    void setupAboutTab();
    
    void loadSettings();
    void saveSettings();
    void updateModelList();
    void updatePromptList();
    
    ConfigManager* m_configManager;
    
    // UI 组件
    QTabWidget* m_tabWidget;
    
    // 模型配置标签页
    QWidget* m_modelTab;
    QMap<QString, QLineEdit*> m_providerKeyEdits;  // 动态创建的 Provider API Key 输入框
    QMap<QString, QLineEdit*> m_providerUrlEdits;  // 动态创建的 Provider API URL 输入框
    QTabWidget* m_modelCategoryTabs;  // 模型分类选项卡
    QMap<QString, QListWidget*> m_providerModelLists;  // 按 provider 分组的模型列表
    QListWidget* m_localModelList;    // 离线模型列表（最后一个选项卡）
    QPushButton* m_addModelBtn;
    QPushButton* m_editModelBtn;
    QPushButton* m_removeModelBtn;
    
    // 获取当前选中的模型列表
    QListWidget* getCurrentModelList() const;
    
    // 创建或获取指定 provider 的列表
    QListWidget* getOrCreateProviderList(const QString& providerId, const QString& providerName);
    
    // 更新选项卡（根据模型动态创建）
    void updateModelTabs();
    
    // 提示词模板标签页
    QWidget* m_promptTab;
    QTabWidget* m_promptCategoryTabs;  // 提示词分类选项卡
    QMap<QString, QListWidget*> m_promptTypeLists;  // 按类型分组的提示词列表
    QPushButton* m_addPromptBtn;
    QPushButton* m_editPromptBtn;
    QPushButton* m_removePromptBtn;
    
    // 获取当前选中的提示词列表
    QListWidget* getCurrentPromptList() const;
    
    // 创建或获取指定类型的提示词列表
    QListWidget* getOrCreatePromptTypeList(const QString& type, const QString& typeName);
    
    // 更新提示词选项卡（根据模板动态创建）
    void updatePromptTabs();
    
    // 快捷键标签页
    QWidget* m_shortcutTab;
    QKeySequenceEdit* m_screenshotShortcut;
    QKeySequenceEdit* m_pasteShortcut;
    QKeySequenceEdit* m_recognizeShortcut;
    
    // 通用设置标签页
    QWidget* m_generalTab;
    QCheckBox* m_autoCopyCheck;
    QCheckBox* m_autoSaveCheck;
    QCheckBox* m_autoRecognizeAfterScreenshot;
    QSpinBox* m_maxHistorySpin;
    QLineEdit* m_saveFolderEdit;

    // 关于标签页
    QWidget* m_aboutTab;
    
    // 按钮
    QPushButton* m_saveBtn;
    QPushButton* m_cancelBtn;
    QPushButton* m_applyBtn;
    
    bool m_modified;
};

// 模型编辑对话框
class ModelEditDialog : public QDialog {
    Q_OBJECT
    
public:
    explicit ModelEditDialog(const ModelConfig& config, QWidget* parent = nullptr);
    explicit ModelEditDialog(QWidget* parent = nullptr);
    
    ModelConfig getModelConfig() const;
    
private slots:
    void onTypeChanged(int index);
    void onTestApiKey();
    
private:
    void setupUI();
    void loadConfig(const ModelConfig& config);
    
    QLineEdit* m_idEdit;
    QLineEdit* m_nameEdit;
    QComboBox* m_typeCombo;
    QComboBox* m_engineCombo;
    QComboBox* m_providerCombo;  // 提供商选择
    QCheckBox* m_enabledCheck;
    
    // 引擎特定参数
    QWidget* m_paramsWidget;
    QLabel* m_apiKeyLabel;   // API Key 提示标签
    QLineEdit* m_apiKeyEdit;
    QLineEdit* m_apiUrlEdit;
    QLineEdit* m_modelNameEdit;
    QLineEdit* m_langEdit;
    QLineEdit* m_pathEdit;
    QDoubleSpinBox* m_temperatureEdit;
    QCheckBox* m_enableThinkingCheck;  // 思考模式开关
    QPushButton* m_testApiBtn;
    
    QPushButton* m_okBtn;
    QPushButton* m_cancelBtn;
};

// 提示词模板编辑对话框
class PromptTemplateDialog : public QDialog {
    Q_OBJECT
    
public:
    explicit PromptTemplateDialog(const QString& name, const QString& content, const QString& type, const QString& category, QWidget* parent = nullptr);
    explicit PromptTemplateDialog(QWidget* parent = nullptr);
    
    QString getName() const;
    QString getContent() const;
    QString getType() const;
    QString getCategory() const;
    
private:
    void setupUI();
    
    QLineEdit* m_nameEdit;
    QTextEdit* m_contentEdit;
    QComboBox* m_typeCombo;      // 主分类（识别、翻译、解答）
    QComboBox* m_categoryCombo;  // 子分类
    QPushButton* m_okBtn;
    QPushButton* m_cancelBtn;
};