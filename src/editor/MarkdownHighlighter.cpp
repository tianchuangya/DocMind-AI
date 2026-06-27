// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — MarkdownHighlighter 实现
// ─────────────────────────────────────────────────────────────────────────────
#include "editor/MarkdownHighlighter.h"

namespace dmc {

// ─── 预编译正则 ──────────────────────────────────────────────────────────────
const QRegularExpression MarkdownHighlighter::sm_headingRe(R"(^(#{1,6})\s+(.+)$)");
const QRegularExpression MarkdownHighlighter::sm_hrRe(R"(^(\*{3,}|-{3,}|_{3,})\s*$)");
const QRegularExpression MarkdownHighlighter::sm_blockquoteRe(R"(^>\s?)");
const QRegularExpression MarkdownHighlighter::sm_ulRe(R"(^(\s*)([*+\-])\s)");
const QRegularExpression MarkdownHighlighter::sm_olRe(R"(^(\s*)(\d+[.)]\s))");
const QRegularExpression MarkdownHighlighter::sm_boldRe(R"(\*\*(.+?)\*\*|__(.+?)__)");
const QRegularExpression MarkdownHighlighter::sm_italicRe(R"((?<!\*)\*(?!\*)(.+?)(?<!\*)\*(?!\*))");
const QRegularExpression MarkdownHighlighter::sm_codeInlineRe(R"(`[^`]+`)");
const QRegularExpression MarkdownHighlighter::sm_linkRe(R"(\[([^\]]+)\]\([^)]+\))");
const QRegularExpression MarkdownHighlighter::sm_imageRe(R"(!\[([^\]]*)\]\([^)]+\))");
const QRegularExpression MarkdownHighlighter::sm_strikeRe(R"(~~.+?~~)");
const QRegularExpression MarkdownHighlighter::sm_taskRe(R"(^\s*[*+\-]\s+\[([ xX])\])");
const QRegularExpression MarkdownHighlighter::sm_htmlEntityRe(R"(&[a-zA-Z]+;|&#[0-9]+;|&#x[0-9a-fA-F]+;)");
const QRegularExpression MarkdownHighlighter::sm_footnoteDefRe(R"(^\[\^[^\]]+\]:)");
const QRegularExpression MarkdownHighlighter::sm_tableRe(R"(^\|.+\|$)");
const QRegularExpression MarkdownHighlighter::sm_fenceRe(R"(^(`{3,}|~{3,}))");

// ─── 构造 / 析构 ─────────────────────────────────────────────────────────────

MarkdownHighlighter::MarkdownHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
    applyLightTheme();
}

MarkdownHighlighter::~MarkdownHighlighter() = default;

void MarkdownHighlighter::setColors(const HighlightColors& colors) {
    m_colors = colors;
    rehighlight();
}

// ─── 默认主题 ────────────────────────────────────────────────────────────────

void MarkdownHighlighter::applyLightTheme() {
    m_colors = {
        .heading       = QColor("#1a5fb4"),
        .bold          = QColor("#c64600"),
        .italic        = QColor("#26a269"),
        .codeInline    = QColor("#813d9c"),
        .codeBlock     = QColor("#613583"),
        .link          = QColor("#1c71d8"),
        .image         = QColor("#2ec27e"),
        .blockquote    = QColor("#77767b"),
        .listMarker    = QColor("#e66100"),
        .horizontalRule= QColor("#9a9996"),
        .htmlEntity    = QColor("#c64600"),
        .taskChecked   = QColor("#26a269"),
        .taskUnchecked = QColor("#c01c28"),
        .tableBorder   = QColor("#5e5c64"),
        .footnote      = QColor("#865e3c"),
        .strikethrough = QColor("#9a9996"),
        .comment       = QColor("#77767b"),
    };
    rehighlight();
}

void MarkdownHighlighter::applyDarkTheme() {
    m_colors = {
        .heading       = QColor("#62a0ea"),
        .bold          = QColor("#ff7800"),
        .italic        = QColor("#57e389"),
        .codeInline    = QColor("#dc8add"),
        .codeBlock     = QColor("#c061cb"),
        .link          = QColor("#3584e4"),
        .image         = QColor("#33d17a"),
        .blockquote    = QColor("#9a9996"),
        .listMarker    = QColor("#ff7800"),
        .horizontalRule= QColor("#77767b"),
        .htmlEntity    = QColor("#ffa348"),
        .taskChecked   = QColor("#57e389"),
        .taskUnchecked = QColor("#f66151"),
        .tableBorder   = QColor("#c0bfbc"),
        .footnote      = QColor("#cdab8f"),
        .strikethrough = QColor("#77767b"),
        .comment       = QColor("#9a9996"),
    };
    rehighlight();
}

// ─── 主高亮逻辑 ──────────────────────────────────────────────────────────────

void MarkdownHighlighter::highlightBlock(const QString& text) {
    int blockNumber = currentBlock().blockNumber();
    QString trimmed = text.trimmed();

    // ─── 围栏代码块处理 ─────────────────────────────────────────────────
    auto fenceMatch = sm_fenceRe.match(trimmed);
    if (fenceMatch.hasMatch()) {
        if (!m_inCodeBlock) {
            m_inCodeBlock = true;
            m_codeBlockFence = fenceMatch.captured(1).left(1);
        } else if (trimmed.startsWith(m_codeBlockFence.at(0))) {
            m_inCodeBlock = false;
            m_codeBlockFence.clear();
        }
        // 高亮围栏行
        QTextCharFormat fenceFmt;
        fenceFmt.setForeground(m_colors.codeBlock);
        fenceFmt.setFontItalic(true);
        setFormat(0, text.length(), fenceFmt);
        return;
    }

    if (m_inCodeBlock) {
        QTextCharFormat codeFmt;
        codeFmt.setForeground(m_colors.codeBlock);
        setFormat(0, text.length(), codeFmt);
        return;
    }

    // ─── 空行 ───────────────────────────────────────────────────────────
    if (trimmed.isEmpty()) {
        return;
    }

    // ─── 块级高亮 ───────────────────────────────────────────────────────
    highlightFootnoteDef(text);
    highlightHeading(text);
    highlightHorizontalRule(text);
    highlightBlockquote(text);
    highlightListItem(text);
    highlightTable(text);
    highlightTaskItems(text);

    // ─── 内联高亮 ───────────────────────────────────────────────────────
    highlightInlineCode(text);
    highlightImages(text);      // 先处理图片
    highlightLinks(text);
    highlightBold(text);
    highlightItalic(text);
    highlightStrikethrough(text);
    highlightHtmlEntities(text);
}

// ─── 块级高亮方法 ────────────────────────────────────────────────────────────

void MarkdownHighlighter::highlightCodeBlock(const QString& text, int blockNumber) {
    Q_UNUSED(blockNumber);
    QTextCharFormat fmt;
    fmt.setForeground(m_colors.codeBlock);
    setFormat(0, text.length(), fmt);
}

void MarkdownHighlighter::highlightHeading(const QString& text) {
    auto match = sm_headingRe.match(text);
    if (!match.hasMatch()) return;

    QTextCharFormat fmt;
    fmt.setForeground(m_colors.heading);
    fmt.setFontWeight(QFont::Bold);

    int level = match.captured(1).length();
    int size = 16 + (6 - level) * 2;
    fmt.setFontPointSize(size);

    setFormat(0, text.length(), fmt);
}

void MarkdownHighlighter::highlightHorizontalRule(const QString& text) {
    if (!sm_hrRe.match(text).hasMatch()) return;

    QTextCharFormat fmt;
    fmt.setForeground(m_colors.horizontalRule);
    setFormat(0, text.length(), fmt);
}

void MarkdownHighlighter::highlightBlockquote(const QString& text) {
    auto match = sm_blockquoteRe.match(text);
    if (!match.hasMatch()) return;

    QTextCharFormat markerFmt;
    markerFmt.setForeground(m_colors.blockquote);
    markerFmt.setFontWeight(QFont::Bold);
    setFormat(match.capturedStart(), match.capturedLength(), markerFmt);
}

void MarkdownHighlighter::highlightListItem(const QString& text) {
    // 无序列表标记
    auto ulMatch = sm_ulRe.match(text);
    if (ulMatch.hasMatch()) {
        int markerStart = ulMatch.capturedStart(2);
        int markerLen = ulMatch.capturedLength(2);
        QTextCharFormat fmt;
        fmt.setForeground(m_colors.listMarker);
        fmt.setFontWeight(QFont::Bold);
        setFormat(markerStart, markerLen, fmt);
        return;
    }

    // 有序列表标记
    auto olMatch = sm_olRe.match(text);
    if (olMatch.hasMatch()) {
        int markerStart = olMatch.capturedStart(2);
        int markerLen = olMatch.capturedLength(2);
        QTextCharFormat fmt;
        fmt.setForeground(m_colors.listMarker);
        fmt.setFontWeight(QFont::Bold);
        setFormat(markerStart, markerLen, fmt);
    }
}

void MarkdownHighlighter::highlightTable(const QString& text) {
    if (!sm_tableRe.match(text).hasMatch()) return;

    // 高亮 | 分隔符
    QTextCharFormat borderFmt;
    borderFmt.setForeground(m_colors.tableBorder);
    borderFmt.setFontWeight(QFont::Bold);

    for (int i = 0; i < text.length(); i++) {
        if (text[i] == '|') {
            setFormat(i, 1, borderFmt);
        }
    }
}

void MarkdownHighlighter::highlightFootnoteDef(const QString& text) {
    auto match = sm_footnoteDefRe.match(text);
    if (!match.hasMatch()) return;

    QTextCharFormat fmt;
    fmt.setForeground(m_colors.footnote);
    fmt.setFontItalic(true);
    setFormat(match.capturedStart(), match.capturedLength(), fmt);
}

// ─── 内联高亮方法 ────────────────────────────────────────────────────────────

void MarkdownHighlighter::highlightInlineCode(const QString& text) {
    auto it = sm_codeInlineRe.globalMatch(text);
    QTextCharFormat fmt;
    fmt.setForeground(m_colors.codeInline);
    fmt.setFontFamilies(QStringList{"Consolas"});

    while (it.hasNext()) {
        auto match = it.next();
        setFormat(match.capturedStart(), match.capturedLength(), fmt);
    }
}

void MarkdownHighlighter::highlightBold(const QString& text) {
    auto it = sm_boldRe.globalMatch(text);
    QTextCharFormat fmt;
    fmt.setForeground(m_colors.bold);
    fmt.setFontWeight(QFont::Bold);

    while (it.hasNext()) {
        auto match = it.next();
        setFormat(match.capturedStart(), match.capturedLength(), fmt);
    }
}

void MarkdownHighlighter::highlightItalic(const QString& text) {
    auto it = sm_italicRe.globalMatch(text);
    QTextCharFormat fmt;
    fmt.setForeground(m_colors.italic);
    fmt.setFontItalic(true);

    while (it.hasNext()) {
        auto match = it.next();
        setFormat(match.capturedStart(), match.capturedLength(), fmt);
    }
}

void MarkdownHighlighter::highlightLinks(const QString& text) {
    auto it = sm_linkRe.globalMatch(text);
    QTextCharFormat fmt;
    fmt.setForeground(m_colors.link);
    fmt.setUnderlineStyle(QTextCharFormat::SingleUnderline);

    while (it.hasNext()) {
        auto match = it.next();
        setFormat(match.capturedStart(), match.capturedLength(), fmt);
    }
}

void MarkdownHighlighter::highlightImages(const QString& text) {
    auto it = sm_imageRe.globalMatch(text);
    QTextCharFormat fmt;
    fmt.setForeground(m_colors.image);
    fmt.setFontItalic(true);

    while (it.hasNext()) {
        auto match = it.next();
        setFormat(match.capturedStart(), match.capturedLength(), fmt);
    }
}

void MarkdownHighlighter::highlightStrikethrough(const QString& text) {
    auto it = sm_strikeRe.globalMatch(text);
    QTextCharFormat fmt;
    fmt.setForeground(m_colors.strikethrough);
    fmt.setFontStrikeOut(true);

    while (it.hasNext()) {
        auto match = it.next();
        setFormat(match.capturedStart(), match.capturedLength(), fmt);
    }
}

void MarkdownHighlighter::highlightTaskItems(const QString& text) {
    auto match = sm_taskRe.match(text);
    if (!match.hasMatch()) return;

    bool checked = (match.captured(1) == "x" || match.captured(1) == "X");
    QColor color = checked ? m_colors.taskChecked : m_colors.taskUnchecked;

    QTextCharFormat fmt;
    fmt.setForeground(color);
    fmt.setFontWeight(QFont::Bold);
    setFormat(match.capturedStart(), match.capturedLength(), fmt);
}

void MarkdownHighlighter::highlightHtmlEntities(const QString& text) {
    auto it = sm_htmlEntityRe.globalMatch(text);
    QTextCharFormat fmt;
    fmt.setForeground(m_colors.htmlEntity);

    while (it.hasNext()) {
        auto match = it.next();
        setFormat(match.capturedStart(), match.capturedLength(), fmt);
    }
}

// ─── 辅助方法 ────────────────────────────────────────────────────────────────

void MarkdownHighlighter::setFormat(int start, int count, const QTextCharFormat& format) {
    QSyntaxHighlighter::setFormat(start, count, format);
}

void MarkdownHighlighter::applyFormat(int start, int count, const QColor& color,
                                        bool bold, bool italic) {
    QTextCharFormat fmt;
    fmt.setForeground(color);
    if (bold) fmt.setFontWeight(QFont::Bold);
    if (italic) fmt.setFontItalic(true);
    setFormat(start, count, fmt);
}

} // namespace dmc
