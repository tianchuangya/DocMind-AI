// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — FindReplaceDialog 实现
// ─────────────────────────────────────────────────────────────────────────────
#include "editor/FindReplaceDialog.h"
#include "editor/MarkdownEditor.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QKeyEvent>
#include <QShowEvent>

namespace dmc {

FindReplaceDialog::FindReplaceDialog(MarkdownEditor* editor, QWidget* parent)
    : QDialog(parent)
    , m_editor(editor)
{
    setupUI();
    setWindowTitle("查找");
    setMinimumWidth(420);
}

FindReplaceDialog::~FindReplaceDialog() = default;

void FindReplaceDialog::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    // 查找行
    auto* findLayout = new QHBoxLayout();
    findLayout->addWidget(new QLabel("查找:"));
    m_findEdit = new QLineEdit();
    m_findEdit->setPlaceholderText("输入查找内容...");
    m_findEdit->setClearButtonEnabled(true);
    findLayout->addWidget(m_findEdit);
    mainLayout->addLayout(findLayout);

    // 替换行（可隐藏）
    m_replaceWidget = new QWidget();
    auto* replaceLayout = new QHBoxLayout(m_replaceWidget);
    replaceLayout->setContentsMargins(0, 0, 0, 0);
    replaceLayout->addWidget(new QLabel("替换:"));
    m_replaceEdit = new QLineEdit();
    m_replaceEdit->setPlaceholderText("输入替换内容...");
    m_replaceEdit->setClearButtonEnabled(true);
    replaceLayout->addWidget(m_replaceEdit);
    mainLayout->addWidget(m_replaceWidget);

    // 选项行
    auto* optLayout = new QHBoxLayout();
    m_caseCheck  = new QCheckBox("区分大小写");
    m_wholeWord  = new QCheckBox("全字匹配");
    m_regexCheck = new QCheckBox("正则表达式");
    optLayout->addWidget(m_caseCheck);
    optLayout->addWidget(m_wholeWord);
    optLayout->addWidget(m_regexCheck);
    optLayout->addStretch();
    mainLayout->addLayout(optLayout);

    // 按钮行
    auto* btnLayout = new QHBoxLayout();
    m_findNextBtn   = new QPushButton("下一个");
    m_findPrevBtn   = new QPushButton("上一个");
    m_replaceBtn    = new QPushButton("替换");
    m_replaceAllBtn = new QPushButton("全部替换");
    m_resultLabel   = new QLabel("");

    btnLayout->addWidget(m_findPrevBtn);
    btnLayout->addWidget(m_findNextBtn);
    btnLayout->addSpacing(12);
    btnLayout->addWidget(m_replaceBtn);
    btnLayout->addWidget(m_replaceAllBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_resultLabel);
    mainLayout->addLayout(btnLayout);

    // 连接信号
    connect(m_findEdit, &QLineEdit::textChanged,
            this, &FindReplaceDialog::onFindTextChanged);
    connect(m_findEdit, &QLineEdit::returnPressed,
            this, &FindReplaceDialog::findNext);
    connect(m_findNextBtn, &QPushButton::clicked,
            this, &FindReplaceDialog::findNext);
    connect(m_findPrevBtn, &QPushButton::clicked,
            this, &FindReplaceDialog::findPrevious);
    connect(m_replaceBtn, &QPushButton::clicked,
            this, &FindReplaceDialog::replace);
    connect(m_replaceAllBtn, &QPushButton::clicked,
            this, &FindReplaceDialog::replaceAll);

    // 默认隐藏替换
    m_replaceWidget->hide();
    m_replaceBtn->hide();
    m_replaceAllBtn->hide();
}

void FindReplaceDialog::showFindOnly() {
    m_showReplace = false;
    m_replaceWidget->hide();
    m_replaceBtn->hide();
    m_replaceAllBtn->hide();
    setWindowTitle("查找");
    show();
    m_findEdit->setFocus();
    m_findEdit->selectAll();
}

void FindReplaceDialog::showFindAndReplace() {
    m_showReplace = true;
    m_replaceWidget->show();
    m_replaceBtn->show();
    m_replaceAllBtn->show();
    setWindowTitle("查找和替换");
    show();
    m_findEdit->setFocus();
    m_findEdit->selectAll();
}

void FindReplaceDialog::setFindText(const QString& text) {
    m_findEdit->setText(text);
}

void FindReplaceDialog::findNext() {
    if (m_findEdit->text().isEmpty()) return;
    int result = m_editor->findText(
        m_findEdit->text(),
        m_caseCheck->isChecked(),
        m_wholeWord->isChecked(),
        m_regexCheck->isChecked(),
        true
    );
    updateResultLabel(result);
}

void FindReplaceDialog::findPrevious() {
    if (m_findEdit->text().isEmpty()) return;
    int result = m_editor->findText(
        m_findEdit->text(),
        m_caseCheck->isChecked(),
        m_wholeWord->isChecked(),
        m_regexCheck->isChecked(),
        false
    );
    updateResultLabel(result);
}

void FindReplaceDialog::replace() {
    if (m_findEdit->text().isEmpty()) return;
    int result = m_editor->replaceText(
        m_findEdit->text(),
        m_replaceEdit->text(),
        m_caseCheck->isChecked(),
        m_wholeWord->isChecked(),
        m_regexCheck->isChecked()
    );
    updateResultLabel(result);
}

void FindReplaceDialog::replaceAll() {
    if (m_findEdit->text().isEmpty()) return;
    int count = m_editor->replaceAll(
        m_findEdit->text(),
        m_replaceEdit->text(),
        m_caseCheck->isChecked(),
        m_wholeWord->isChecked(),
        m_regexCheck->isChecked()
    );
    m_resultLabel->setText(QString("已替换 %1 处").arg(count));
}

void FindReplaceDialog::onFindTextChanged(const QString& text) {
    bool empty = text.isEmpty();
    m_findNextBtn->setEnabled(!empty);
    m_findPrevBtn->setEnabled(!empty);
    m_replaceBtn->setEnabled(!empty);
    m_replaceAllBtn->setEnabled(!empty);
    m_resultLabel->clear();
}

void FindReplaceDialog::updateResultLabel(int count) {
    if (count > 0) {
        m_resultLabel->setText("已找到");
        m_resultLabel->setStyleSheet("color: green;");
    } else {
        m_resultLabel->setText("未找到");
        m_resultLabel->setStyleSheet("color: red;");
    }
}

void FindReplaceDialog::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        hide();
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        findNext();
        return;
    }
    QDialog::keyPressEvent(event);
}

void FindReplaceDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    // 将选中文本自动填入查找框
    QString selected = m_editor->selectedText();
    if (!selected.isEmpty()) {
        m_findEdit->setText(selected);
        m_findEdit->selectAll();
    } else {
        m_findEdit->selectAll();
    }
}

} // namespace dmc
