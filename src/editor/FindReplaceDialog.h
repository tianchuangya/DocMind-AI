// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — FindReplaceDialog
// 查找与替换对话框
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <QDialog>

class QLineEdit;
class QCheckBox;
class QPushButton;
class QLabel;

namespace dmc {

class MarkdownEditor;

class FindReplaceDialog : public QDialog {
    Q_OBJECT

public:
    explicit FindReplaceDialog(MarkdownEditor* editor, QWidget* parent = nullptr);
    ~FindReplaceDialog() override;

    /// 显示为查找模式
    void showFindOnly();

    /// 显示为查找替换模式
    void showFindAndReplace();

    /// 设置查找文本
    void setFindText(const QString& text);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void findNext();
    void findPrevious();
    void replace();
    void replaceAll();
    void onFindTextChanged(const QString& text);

private:
    void setupUI();
    void updateResultLabel(int count);

    MarkdownEditor* m_editor;

    QLineEdit*   m_findEdit    = nullptr;
    QLineEdit*   m_replaceEdit = nullptr;
    QCheckBox*   m_caseCheck   = nullptr;
    QCheckBox*   m_wholeWord   = nullptr;
    QCheckBox*   m_regexCheck  = nullptr;
    QPushButton* m_findNextBtn  = nullptr;
    QPushButton* m_findPrevBtn  = nullptr;
    QPushButton* m_replaceBtn   = nullptr;
    QPushButton* m_replaceAllBtn= nullptr;
    QLabel*      m_resultLabel = nullptr;

    QWidget*     m_replaceWidget = nullptr;
    bool         m_showReplace   = false;
};

} // namespace dmc
