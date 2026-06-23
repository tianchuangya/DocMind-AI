// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — SettingsDialog
// 设置对话框（编辑器字体、Tab大小、自动保存、主题等）
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <QDialog>
#include "core/AppState.h"

class QSpinBox;
class QCheckBox;
class QFontComboBox;
class QComboBox;

namespace dmc {

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog() override;

private slots:
    void apply();
    void ok();
    void cancel();
    void reset();

private:
    void setupUI();
    void loadSettings();
    void saveSettings();

    QFontComboBox* m_fontCombo    = nullptr;
    QSpinBox*      m_fontSizeSpin = nullptr;
    QSpinBox*      m_tabSizeSpin  = nullptr;
    QCheckBox*     m_wordWrap     = nullptr;
    QCheckBox*     m_lineNumbers  = nullptr;
    QCheckBox*     m_autoSave     = nullptr;
    QSpinBox*      m_autoSaveInterval = nullptr;
    QComboBox*     m_viewMode     = nullptr;
    QComboBox*     m_themeMode    = nullptr;
};

} // namespace dmc
