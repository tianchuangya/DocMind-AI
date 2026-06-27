// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — SettingsDialog 实现
// ─────────────────────────────────────────────────────────────────────────────
#include "app/SettingsDialog.h"
#include "core/AppState.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QCheckBox>
#include <QFontComboBox>
#include <QComboBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QTabWidget>

namespace dmc {

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("设置");
    setMinimumSize(500, 400);
    setupUI();
    loadSettings();
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);

    auto* tabWidget = new QTabWidget();

    // ─── 编辑器设置 ─────────────────────────────────────────────────────
    auto* editorPage = new QWidget();
    auto* editorLayout = new QGridLayout(editorPage);
    editorLayout->setSpacing(12);

    int row = 0;

    // 字体
    editorLayout->addWidget(new QLabel("编辑器字体:"), row, 0);
    m_fontCombo = new QFontComboBox();
    m_fontCombo->setFontFilters(QFontComboBox::MonospacedFonts);
    editorLayout->addWidget(m_fontCombo, row, 1);
    row++;

    // 字号
    editorLayout->addWidget(new QLabel("字号:"), row, 0);
    m_fontSizeSpin = new QSpinBox();
    m_fontSizeSpin->setRange(6, 72);
    m_fontSizeSpin->setSuffix(" pt");
    editorLayout->addWidget(m_fontSizeSpin, row, 1);
    row++;

    // Tab 大小
    editorLayout->addWidget(new QLabel("Tab 缩进:"), row, 0);
    m_tabSizeSpin = new QSpinBox();
    m_tabSizeSpin->setRange(1, 8);
    m_tabSizeSpin->setSuffix(" 空格");
    editorLayout->addWidget(m_tabSizeSpin, row, 1);
    row++;

    // 自动换行
    m_wordWrap = new QCheckBox("自动换行");
    editorLayout->addWidget(m_wordWrap, row, 0, 1, 2);
    row++;

    // 显示行号
    m_lineNumbers = new QCheckBox("显示行号");
    editorLayout->addWidget(m_lineNumbers, row, 0, 1, 2);
    row++;

    editorLayout->setRowStretch(row, 1);
    tabWidget->addTab(editorPage, "编辑器");

    // ─── 保存设置 ───────────────────────────────────────────────────────
    auto* savePage = new QWidget();
    auto* saveLayout = new QGridLayout(savePage);
    saveLayout->setSpacing(12);

    row = 0;
    m_autoSave = new QCheckBox("启用自动保存");
    saveLayout->addWidget(m_autoSave, row, 0, 1, 2);
    row++;

    saveLayout->addWidget(new QLabel("保存间隔:"), row, 0);
    m_autoSaveInterval = new QSpinBox();
    m_autoSaveInterval->setRange(10, 600);
    m_autoSaveInterval->setSuffix(" 秒");
    saveLayout->addWidget(m_autoSaveInterval, row, 1);
    row++;

    saveLayout->setRowStretch(row, 1);
    tabWidget->addTab(savePage, "保存");

    // ─── 视图设置 ───────────────────────────────────────────────────────
    auto* viewPage = new QWidget();
    auto* viewLayout = new QGridLayout(viewPage);
    viewLayout->setSpacing(12);

    row = 0;
    viewLayout->addWidget(new QLabel("默认视图模式:"), row, 0);
    m_viewMode = new QComboBox();
    m_viewMode->addItem("仅编辑器",    static_cast<int>(ViewMode::EditorOnly));
    m_viewMode->addItem("分栏视图",    static_cast<int>(ViewMode::SplitView));
    m_viewMode->addItem("仅预览",      static_cast<int>(ViewMode::PreviewOnly));
    viewLayout->addWidget(m_viewMode, row, 1);
    row++;

    viewLayout->addWidget(new QLabel("主题模式:"), row, 0);
    m_themeMode = new QComboBox();
    m_themeMode->addItem("浅色主题", static_cast<int>(ThemeMode::Light));
    m_themeMode->addItem("深色主题", static_cast<int>(ThemeMode::Dark));
    viewLayout->addWidget(m_themeMode, row, 1);
    row++;

    viewLayout->setRowStretch(row, 1);
    tabWidget->addTab(viewPage, "视图");

    mainLayout->addWidget(tabWidget);

    // ─── 按钮 ───────────────────────────────────────────────────────────
    auto* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply |
        QDialogButtonBox::RestoreDefaults);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::ok);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::cancel);
    connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &SettingsDialog::apply);
    connect(buttonBox->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked,
            this, &SettingsDialog::reset);

    mainLayout->addWidget(buttonBox);
}

void SettingsDialog::loadSettings() {
    auto& state = AppState::instance();

    m_fontCombo->setCurrentFont(state.editorFont());
    m_fontSizeSpin->setValue(state.editorFont().pointSize());
    m_tabSizeSpin->setValue(state.tabSize());
    m_wordWrap->setChecked(state.wordWrap());
    m_lineNumbers->setChecked(state.showLineNumbers());
    m_autoSave->setChecked(state.autoSaveEnabled());
    m_autoSaveInterval->setValue(state.autoSaveInterval());

    // 视图
    m_viewMode->setCurrentIndex(
        m_viewMode->findData(static_cast<int>(state.viewMode())));
    m_themeMode->setCurrentIndex(
        m_themeMode->findData(static_cast<int>(state.themeMode())));
}

void SettingsDialog::saveSettings() {
    auto& state = AppState::instance();

    QFont font = m_fontCombo->currentFont();
    font.setPointSize(m_fontSizeSpin->value());
    state.setEditorFont(font);
    state.setTabSize(m_tabSizeSpin->value());
    state.setWordWrap(m_wordWrap->isChecked());
    state.setShowLineNumbers(m_lineNumbers->isChecked());
    state.setAutoSaveEnabled(m_autoSave->isChecked());
    state.setAutoSaveInterval(m_autoSaveInterval->value());

    state.setViewMode(static_cast<ViewMode>(m_viewMode->currentData().toInt()));
    state.setThemeMode(static_cast<ThemeMode>(m_themeMode->currentData().toInt()));

    state.saveSettings();
}

void SettingsDialog::apply() {
    saveSettings();
}

void SettingsDialog::ok() {
    saveSettings();
    accept();
}

void SettingsDialog::cancel() {
    reject();
}

void SettingsDialog::reset() {
    m_fontCombo->setCurrentFont(QFont("Consolas", 12));
    m_fontSizeSpin->setValue(12);
    m_tabSizeSpin->setValue(4);
    m_wordWrap->setChecked(true);
    m_lineNumbers->setChecked(true);
    m_autoSave->setChecked(true);
    m_autoSaveInterval->setValue(60);
    m_viewMode->setCurrentIndex(1);
    m_themeMode->setCurrentIndex(0);
}

} // namespace dmc
