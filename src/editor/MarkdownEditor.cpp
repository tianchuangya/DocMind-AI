// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — MarkdownEditor 实现
// ─────────────────────────────────────────────────────────────────────────────
#include "editor/MarkdownEditor.h"
#include "editor/LineNumberArea.h"
#include "editor/MarkdownHighlighter.h"
#include "core/AppState.h"

#include <QPainter>
#include <QTextBlock>
#include <QScrollBar>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QFontMetrics>
#include <QWheelEvent>
#include <QAbstractTextDocumentLayout>

namespace dmc {

MarkdownEditor::MarkdownEditor(QWidget* parent)
    : QPlainTextEdit(parent)
{
    // 行号区域
    m_lineNumberArea = new LineNumberArea(this);

    // 语法高亮
    m_highlighter = new MarkdownHighlighter(document());

    // 防抖计时器（300ms 后触发重渲染）
    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(300);

    // 基础配置
    setTabStopDistance(tabStopDistance());
    setLineWrapMode(QPlainTextEdit::WidgetWidth);

    // 应用默认字体
    setupFont();
    setupConnections();

    // 初始更新行号宽度
    updateLineNumberAreaWidth();
}

MarkdownEditor::~MarkdownEditor() = default;

// ─── 配置方法 ────────────────────────────────────────────────────────────────

void MarkdownEditor::setTabSize(int size) {
    m_tabSize = size;
    setTabStopDistance(fontMetrics().horizontalAdvance(' ') * m_tabSize);
    updateLineNumberAreaWidth();
}

void MarkdownEditor::setShowLineNumbers(bool show) {
    m_showLineNumbers = show;
    m_lineNumberArea->setVisible(show);
    updateLineNumberAreaWidth();
}

void MarkdownEditor::setWordWrap(bool enabled) {
    setLineWrapMode(enabled ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
}

bool MarkdownEditor::wordWrap() const {
    return lineWrapMode() != QPlainTextEdit::NoWrap;
}

void MarkdownEditor::setupFont() {
    QFont font = AppState::instance().editorFont();
    setFont(font);
    setTabStopDistance(fontMetrics().horizontalAdvance(' ') * m_tabSize);
}

void MarkdownEditor::setupConnections() {
    connect(this, &QPlainTextEdit::blockCountChanged,
            this, &MarkdownEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest,
            this, &MarkdownEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged,
            this, &MarkdownEditor::onCursorPositionChanged);
    connect(this, &QPlainTextEdit::textChanged,
            this, &MarkdownEditor::onTextChanged);
    connect(&m_debounceTimer, &QTimer::timeout,
            this, &MarkdownEditor::onDebounceTimeout);

    // 监听全局字体变更
    connect(&AppState::instance(), &AppState::editorFontChanged, this, [this](const QFont& font) {
        setFont(font);
        setTabStopDistance(fontMetrics().horizontalAdvance(' ') * m_tabSize);
        updateLineNumberAreaWidth();
    });
}

// ─── 行号区域 ────────────────────────────────────────────────────────────────

int MarkdownEditor::lineNumberAreaWidth() const {
    if (!m_showLineNumbers) return 0;

    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    digits = qMax(digits, 3);

    int space = 12 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void MarkdownEditor::updateLineNumberAreaWidth() {
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void MarkdownEditor::updateLineNumberArea(const QRect& rect, int dy) {
    if (dy) {
        m_lineNumberArea->scroll(0, dy);
    } else {
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());
    }

    if (rect.contains(viewport()->rect())) {
        updateLineNumberAreaWidth();
    }
}

void MarkdownEditor::paintLineNumberArea(LineNumberArea* area, QPaintEvent* event) {
    if (!m_showLineNumbers) return;

    QPainter painter(area);

    // 背景色
    QColor bgColor = palette().color(QPalette::Window).darker(105);
    painter.fillRect(event->rect(), bgColor);

    // 分隔线
    QColor lineColor = palette().color(QPalette::Mid);
    painter.setPen(lineColor);
    painter.drawLine(area->rect().right(), 0, area->rect().right(), area->rect().bottom());

    // 行号文字
    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    QColor normalColor = palette().color(QPalette::Text).darker(130);
    QColor currentColor = palette().color(QPalette::Highlight);

    int currentBlockNum = textCursor().blockNumber();

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);

            bool isCurrent = (blockNumber == currentBlockNum);
            painter.setPen(isCurrent ? currentColor : normalColor);

            QFont f = font();
            if (isCurrent) f.setBold(true);
            painter.setFont(f);

            painter.drawText(0, top, area->width() - 6, fontMetrics().height(),
                           Qt::AlignRight | Qt::AlignVCenter, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

// ─── 编辑器状态 ──────────────────────────────────────────────────────────────

EditorState MarkdownEditor::currentState() const {
    EditorState state;
    QTextCursor cursor = textCursor();
    state.cursorLine     = cursor.blockNumber() + 1;
    state.cursorColumn   = cursor.columnNumber() + 1;
    state.selectionLength = cursor.selectedText().length();
    state.overwriteMode   = overwriteMode();
    return state;
}

int MarkdownEditor::currentLine() const {
    return textCursor().blockNumber() + 1;
}

int MarkdownEditor::currentColumn() const {
    return textCursor().columnNumber() + 1;
}

QString MarkdownEditor::selectedText() const {
    return textCursor().selectedText();
}

// ─── Markdown 快捷操作 ──────────────────────────────────────────────────────

void MarkdownEditor::insertHeading(int level) {
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::StartOfBlock);

    // 检查当前行是否已有标题标记
    QString blockText = cursor.block().text();
    QRegularExpression headingRe(R"(^(#{1,6})\s)");
    auto match = headingRe.match(blockText);

    if (match.hasMatch()) {
        cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor,
                           match.capturedLength(1) + 1);
        cursor.removeSelectedText();
    }

    cursor.insertText(QString(level, '#') + " ");
    setTextCursor(cursor);
}

void MarkdownEditor::insertBold() {
    QTextCursor cursor = textCursor();
    if (cursor.hasSelection()) {
        QString selected = cursor.selectedText();
        cursor.insertText("**" + selected + "**");
    } else {
        cursor.insertText("****");
        cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, 2);
        setTextCursor(cursor);
    }
}

void MarkdownEditor::insertItalic() {
    QTextCursor cursor = textCursor();
    if (cursor.hasSelection()) {
        QString selected = cursor.selectedText();
        cursor.insertText("*" + selected + "*");
    } else {
        cursor.insertText("**");
        cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, 1);
        setTextCursor(cursor);
    }
}

void MarkdownEditor::insertStrikethrough() {
    QTextCursor cursor = textCursor();
    if (cursor.hasSelection()) {
        QString selected = cursor.selectedText();
        cursor.insertText("~~" + selected + "~~");
    } else {
        cursor.insertText("~~~~");
        cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, 2);
        setTextCursor(cursor);
    }
}

void MarkdownEditor::insertCode() {
    QTextCursor cursor = textCursor();
    if (cursor.hasSelection()) {
        QString selected = cursor.selectedText();
        cursor.insertText("`" + selected + "`");
    } else {
        cursor.insertText("``");
        cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, 1);
        setTextCursor(cursor);
    }
}

void MarkdownEditor::insertCodeBlock() {
    QTextCursor cursor = textCursor();
    cursor.insertText("\n```\n\n```\n");
    cursor.movePosition(QTextCursor::Up, QTextCursor::MoveAnchor, 2);
    setTextCursor(cursor);
}

void MarkdownEditor::insertLink() {
    QTextCursor cursor = textCursor();
    if (cursor.hasSelection()) {
        QString selected = cursor.selectedText();
        cursor.insertText("[" + selected + "](url)");
    } else {
        cursor.insertText("[文本](url)");
    }
}

void MarkdownEditor::insertImage() {
    QTextCursor cursor = textCursor();
    cursor.insertText("![图片描述](image_path)");
}

void MarkdownEditor::insertTable(int rows, int cols) {
    QTextCursor cursor = textCursor();
    QString table = "\n";

    // 表头
    table += "|";
    for (int c = 0; c < cols; c++) {
        table += " 标题" + QString::number(c + 1) + " |";
    }
    table += "\n|";

    // 分隔行
    for (int c = 0; c < cols; c++) {
        table += " --- |";
    }
    table += "\n";

    // 数据行
    for (int r = 0; r < rows; r++) {
        table += "|";
        for (int c = 0; c < cols; c++) {
            table += "  |";
        }
        table += "\n";
    }

    cursor.insertText(table);
    setTextCursor(cursor);
}

void MarkdownEditor::insertTaskList() {
    QTextCursor cursor = textCursor();
    cursor.insertText("\n- [ ] 任务项\n- [ ] 任务项\n- [x] 已完成\n");
}

void MarkdownEditor::insertBlockquote() {
    QTextCursor cursor = textCursor();
    if (cursor.hasSelection()) {
        QStringList lines = cursor.selectedText().split('\n');
        QString quoted;
        for (const QString& line : lines) {
            quoted += "> " + line + "\n";
        }
        cursor.insertText(quoted);
    } else {
        cursor.insertText("\n> ");
    }
}

void MarkdownEditor::insertHorizontalRule() {
    QTextCursor cursor = textCursor();
    cursor.insertText("\n---\n");
}

void MarkdownEditor::insertOrderedList() {
    QTextCursor cursor = textCursor();
    cursor.insertText("\n1. 第一项\n2. 第二项\n3. 第三项\n");
}

void MarkdownEditor::insertUnorderedList() {
    QTextCursor cursor = textCursor();
    cursor.insertText("\n- 项目一\n- 项目二\n- 项目三\n");
}

// ─── 搜索替换 ────────────────────────────────────────────────────────────────

int MarkdownEditor::findText(const QString& text, bool caseSensitive,
                              bool wholeWord, bool regex, bool forward) {
    if (text.isEmpty()) return 0;

    QTextDocument::FindFlags flags;
    if (!forward) flags |= QTextDocument::FindBackward;
    if (caseSensitive) flags |= QTextDocument::FindCaseSensitively;
    if (wholeWord) flags |= QTextDocument::FindWholeWords;

    QTextCursor cursor = textCursor();
    bool found;

    if (regex) {
        QRegularExpression re(text, caseSensitive
                              ? QRegularExpression::NoPatternOption
                              : QRegularExpression::CaseInsensitiveOption);
        if (wholeWord) {
            re.setPattern("\\b" + re.pattern() + "\\b");
        }
        cursor = document()->find(re, cursor, flags);
        found = !cursor.isNull();
    } else {
        cursor = document()->find(text, cursor, flags);
        found = !cursor.isNull();
    }

    if (found) {
        setTextCursor(cursor);
        return 1;
    }

    // 未找到 → 从头/尾重新搜索
    QTextCursor restart = textCursor();
    restart.movePosition(forward ? QTextCursor::Start : QTextCursor::End);

    if (regex) {
        QRegularExpression re(text, caseSensitive
                              ? QRegularExpression::NoPatternOption
                              : QRegularExpression::CaseInsensitiveOption);
        cursor = document()->find(re, restart, flags);
    } else {
        cursor = document()->find(text, restart, flags);
    }

    if (!cursor.isNull()) {
        setTextCursor(cursor);
        return 1;
    }

    return 0;
}

int MarkdownEditor::replaceText(const QString& find, const QString& replace,
                                 bool caseSensitive, bool wholeWord, bool regex) {
    QTextCursor cursor = textCursor();
    if (!cursor.hasSelection()) {
        return findText(find, caseSensitive, wholeWord, regex);
    }

    // 验证选中文本是否匹配
    QString selected = cursor.selectedText();
    bool matches = false;

    if (regex) {
        QRegularExpression re(find, caseSensitive
                              ? QRegularExpression::NoPatternOption
                              : QRegularExpression::CaseInsensitiveOption);
        matches = re.match(selected).hasMatch();
    } else {
        if (caseSensitive) {
            matches = (selected == find);
        } else {
            matches = (selected.compare(find, Qt::CaseInsensitive) == 0);
        }
    }

    if (matches) {
        cursor.insertText(replace);
    }

    return findText(find, caseSensitive, wholeWord, regex);
}

int MarkdownEditor::replaceAll(const QString& find, const QString& replace,
                                bool caseSensitive, bool wholeWord, bool regex) {
    int count = 0;
    QTextCursor cursor(document());
    cursor.movePosition(QTextCursor::Start);

    QTextDocument::FindFlags flags;
    if (caseSensitive) flags |= QTextDocument::FindCaseSensitively;
    if (wholeWord) flags |= QTextDocument::FindWholeWords;

    while (true) {
        QTextCursor found;
        if (regex) {
            QRegularExpression re(find, caseSensitive
                                  ? QRegularExpression::NoPatternOption
                                  : QRegularExpression::CaseInsensitiveOption);
            found = document()->find(re, cursor, flags);
        } else {
            found = document()->find(find, cursor, flags);
        }

        if (found.isNull()) break;

        found.insertText(replace);
        cursor = found;
        count++;
    }

    return count;
}

// ─── 主题 ────────────────────────────────────────────────────────────────────

void MarkdownEditor::applyLightTheme() {
    m_highlighter->applyLightTheme();
}

void MarkdownEditor::applyDarkTheme() {
    m_highlighter->applyDarkTheme();
}

// ─── 事件处理 ────────────────────────────────────────────────────────────────

void MarkdownEditor::resizeEvent(QResizeEvent* event) {
    QPlainTextEdit::resizeEvent(event);

    QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(),
                                         lineNumberAreaWidth(), cr.height()));
}

void MarkdownEditor::keyPressEvent(QKeyEvent* event) {
    // Tab → 插入空格或缩进
    if (event->key() == Qt::Key_Tab && !event->modifiers()) {
        if (textCursor().hasSelection()) {
            // 多行缩进
            QTextCursor cursor = textCursor();
            int start = cursor.selectionStart();
            int end   = cursor.selectionEnd();

            cursor.beginEditBlock();
            cursor.setPosition(start);
            cursor.movePosition(QTextCursor::StartOfBlock);

            while (cursor.position() <= end) {
                cursor.insertText(tabString());
                end += m_tabSize;
                if (!cursor.movePosition(QTextCursor::NextBlock)) break;
            }
            cursor.endEditBlock();
        } else {
            handleTabKey();
        }
        return;
    }

    // Shift+Tab → 反向缩进
    if (event->key() == Qt::Key_Backtab) {
        handleBacktabKey();
        return;
    }

    // Enter → 自动缩进和列表延续
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        handleEnterKey();
        return;
    }

    // Ctrl+Enter → 插入空行
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        && event->modifiers() & Qt::ControlModifier) {
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::EndOfBlock);
        cursor.insertText("\n\n");
        setTextCursor(cursor);
        return;
    }

    QPlainTextEdit::keyPressEvent(event);
}

void MarkdownEditor::wheelEvent(QWheelEvent* event) {
    // Ctrl + 滚轮 → 缩放字体
    if (event->modifiers() & Qt::ControlModifier) {
        int delta = event->angleDelta().y();
        QFont f = font();
        int size = f.pointSize();

        if (delta > 0 && size < 72) {
            f.setPointSize(size + 1);
        } else if (delta < 0 && size > 6) {
            f.setPointSize(size - 1);
        }

        setFont(f);
        setTabStopDistance(fontMetrics().horizontalAdvance(' ') * m_tabSize);
        updateLineNumberAreaWidth();

        AppState::instance().setEditorFont(f);
        event->accept();
        return;
    }

    QPlainTextEdit::wheelEvent(event);
}

// ─── 私有方法 ────────────────────────────────────────────────────────────────

void MarkdownEditor::onCursorPositionChanged() {
    QTextCursor cursor = textCursor();
    emit cursorInfoChanged(cursor.blockNumber() + 1,
                           cursor.columnNumber() + 1,
                           cursor.selectedText().length());
}

void MarkdownEditor::onTextChanged() {
    m_debounceTimer.start();
    emit contentModified();
}

void MarkdownEditor::onDebounceTimeout() {
    emit lineNumberAreaUpdateRequested();
}

QString MarkdownEditor::tabString() const {
    if (m_useSpacesForTab) {
        return QString(m_tabSize, ' ');
    }
    return "\t";
}

void MarkdownEditor::handleTabKey() {
    QTextCursor cursor = textCursor();
    cursor.insertText(tabString());
    setTextCursor(cursor);
}

void MarkdownEditor::handleBacktabKey() {
    QTextCursor cursor = textCursor();

    if (cursor.hasSelection()) {
        // 多行反向缩进
        int start = cursor.selectionStart();
        int end   = cursor.selectionEnd();

        cursor.setPosition(start);
        cursor.movePosition(QTextCursor::StartOfBlock);

        while (cursor.position() <= end) {
            QString text = cursor.block().text();
            int removeLen = 0;

            if (text.startsWith(tabString())) {
                removeLen = m_tabSize;
            } else if (text.startsWith("\t")) {
                removeLen = 1;
            } else {
                for (int i = 0; i < m_tabSize && i < text.length() && text[i] == ' '; i++) {
                    removeLen++;
                }
            }

            if (removeLen > 0) {
                cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, removeLen);
                cursor.removeSelectedText();
                end -= removeLen;
            }

            if (!cursor.movePosition(QTextCursor::NextBlock)) break;
        }
    } else {
        // 单行反向缩进
        cursor.movePosition(QTextCursor::StartOfBlock);
        QString text = cursor.block().text();
        int removeLen = 0;

        if (text.startsWith(tabString())) {
            removeLen = m_tabSize;
        } else if (text.startsWith("\t")) {
            removeLen = 1;
        } else {
            for (int i = 0; i < m_tabSize && i < text.length() && text[i] == ' '; i++) {
                removeLen++;
            }
        }

        if (removeLen > 0) {
            cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, removeLen);
            cursor.removeSelectedText();
        }
    }

    setTextCursor(cursor);
}

void MarkdownEditor::handleEnterKey() {
    QTextCursor cursor = textCursor();
    QString currentLine = cursor.block().text();

    // 自动缩进：复制上一行的前导空白
    QString indent;
    QRegularExpression leadingWS(R"(^(\s+))");
    auto wsMatch = leadingWS.match(currentLine);
    if (wsMatch.hasMatch()) {
        indent = wsMatch.captured(1);
    }

    // 列表延续
    QRegularExpression ulRe(R"(^(\s*)([*+\-])\s(.*)$)");
    auto ulMatch = ulRe.match(currentLine);
    if (ulMatch.hasMatch()) {
        QString content = ulMatch.captured(3);
        if (content.trimmed().isEmpty()) {
            // 空列表项 → 结束列表
            cursor.movePosition(QTextCursor::StartOfBlock);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
            cursor.insertText("\n");
            setTextCursor(cursor);
            return;
        }
        cursor.insertText("\n" + ulMatch.captured(1) + ulMatch.captured(2) + " " + indent);
        setTextCursor(cursor);
        return;
    }

    // 有序列表延续
    QRegularExpression olRe(R"(^(\s*)(\d+)[.)]\s(.*)$)");
    auto olMatch = olRe.match(currentLine);
    if (olMatch.hasMatch()) {
        QString content = olMatch.captured(3);
        if (content.trimmed().isEmpty()) {
            cursor.movePosition(QTextCursor::StartOfBlock);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
            cursor.insertText("\n");
            setTextCursor(cursor);
            return;
        }
        int nextNum = olMatch.captured(2).toInt() + 1;
        cursor.insertText("\n" + olMatch.captured(1) + QString::number(nextNum) + ". " + indent);
        setTextCursor(cursor);
        return;
    }

    // 任务列表延续
    QRegularExpression taskRe(R"(^(\s*)([*+\-])\s\[[ xX]\]\s(.*)$)");
    auto taskMatch = taskRe.match(currentLine);
    if (taskMatch.hasMatch()) {
        QString content = taskMatch.captured(3);
        if (content.trimmed().isEmpty()) {
            cursor.movePosition(QTextCursor::StartOfBlock);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
            cursor.insertText("\n");
            setTextCursor(cursor);
            return;
        }
        cursor.insertText("\n" + taskMatch.captured(1) + taskMatch.captured(2) + " [ ] " + indent);
        setTextCursor(cursor);
        return;
    }

    // 引用延续
    QRegularExpression quoteRe(R"(^(\s*>+\s?)(.+)$)");
    auto quoteMatch = quoteRe.match(currentLine);
    if (quoteMatch.hasMatch()) {
        QString content = quoteMatch.captured(2);
        if (content.trimmed().isEmpty()) {
            cursor.movePosition(QTextCursor::StartOfBlock);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
            cursor.insertText("\n");
            setTextCursor(cursor);
            return;
        }
        cursor.insertText("\n" + quoteMatch.captured(1));
        setTextCursor(cursor);
        return;
    }

    // 默认：保留缩进
    cursor.insertText("\n" + indent);
    setTextCursor(cursor);
}

void MarkdownEditor::autoIndent() {
    // 保留当前行的缩进
    QTextCursor cursor = textCursor();
    QString text = cursor.block().text();
    QString indent;

    for (QChar c : text) {
        if (c == ' ' || c == '\t') {
            indent += c;
        } else {
            break;
        }
    }

    cursor.insertText("\n" + indent);
    setTextCursor(cursor);
}

} // namespace dmc
