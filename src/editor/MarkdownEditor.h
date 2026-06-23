// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — MarkdownEditor
// Markdown 编辑器核心控件（基于 QPlainTextEdit）
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <QPlainTextEdit>
#include <QTimer>
#include <memory>

namespace dmc {

class LineNumberArea;
class MarkdownHighlighter;

/// 编辑器状态
struct EditorState {
    int cursorLine   = 1;
    int cursorColumn = 1;
    int selectionLength = 0;
    bool overwriteMode  = false;
};

class MarkdownEditor : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit MarkdownEditor(QWidget* parent = nullptr);
    ~MarkdownEditor() override;

    // ─── 编辑器配置 ───────────────────────────────────────────────────────
    void setTabSize(int size);
    int  tabSize() const { return m_tabSize; }
    void setShowLineNumbers(bool show);
    bool showLineNumbers() const { return m_showLineNumbers; }
    void setWordWrap(bool enabled);
    bool wordWrap() const;

    // ─── 行号区域 ─────────────────────────────────────────────────────────
    int  lineNumberAreaWidth() const;
    void updateLineNumberAreaWidth();
    void paintLineNumberArea(LineNumberArea* area, QPaintEvent* event);

    // ─── 编辑器状态 ───────────────────────────────────────────────────────
    EditorState currentState() const;
    int currentLine() const;
    int currentColumn() const;
    QString selectedText() const;

    // ─── Markdown 快捷操作 ────────────────────────────────────────────────
    void insertHeading(int level);
    void insertBold();
    void insertItalic();
    void insertStrikethrough();
    void insertCode();
    void insertCodeBlock();
    void insertLink();
    void insertImage();
    void insertTable(int rows, int cols);
    void insertTaskList();
    void insertBlockquote();
    void insertHorizontalRule();
    void insertOrderedList();
    void insertUnorderedList();

    // ─── 搜索替换 ─────────────────────────────────────────────────────────
    int  findText(const QString& text, bool caseSensitive = false,
                  bool wholeWord = false, bool regex = false,
                  bool forward = true);
    int  replaceText(const QString& find, const QString& replace,
                     bool caseSensitive = false, bool wholeWord = false,
                     bool regex = false);
    int  replaceAll(const QString& find, const QString& replace,
                    bool caseSensitive = false, bool wholeWord = false,
                    bool regex = false);

    // ─── 高亮 ─────────────────────────────────────────────────────────────
    MarkdownHighlighter* highlighter() const { return m_highlighter; }
    void applyLightTheme();
    void applyDarkTheme();

    // ─── 防抖信号 ─────────────────────────────────────────────────────────
    void setDebounceInterval(int ms) { m_debounceTimer.setInterval(ms); }

signals:
    /// 内容变更（经过防抖）
    void contentModified();
    /// 光标位置变更
    void cursorInfoChanged(int line, int col, int selectionLen);
    /// 请求更新行号区域
    void lineNumberAreaUpdateRequested();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private slots:
    void updateLineNumberArea(const QRect& rect, int dy);
    void onCursorPositionChanged();
    void onTextChanged();
    void onDebounceTimeout();

private:
    void setupConnections();
    void setupFont();
    QString tabString() const;
    void handleTabKey();
    void handleBacktabKey();
    void handleEnterKey();
    void autoIndent();

    // ─── 组件 ─────────────────────────────────────────────────────────────
    LineNumberArea*       m_lineNumberArea = nullptr;
    MarkdownHighlighter*  m_highlighter    = nullptr;

    // ─── 配置 ─────────────────────────────────────────────────────────────
    int  m_tabSize        = 4;
    bool m_showLineNumbers= true;
    bool m_useSpacesForTab= true;

    // ─── 防抖 ─────────────────────────────────────────────────────────────
    QTimer m_debounceTimer;
    qint64 m_lastRenderVersion = 0;
};

} // namespace dmc
