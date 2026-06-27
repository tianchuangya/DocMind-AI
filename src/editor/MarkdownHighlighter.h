// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — MarkdownHighlighter
// Markdown 语法高亮器（基于 QSyntaxHighlighter）
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QColor>
#include <QVector>

namespace dmc {

/// 主题颜色配置
struct HighlightColors {
    QColor heading;
    QColor bold;
    QColor italic;
    QColor codeInline;
    QColor codeBlock;
    QColor link;
    QColor image;
    QColor blockquote;
    QColor listMarker;
    QColor horizontalRule;
    QColor htmlEntity;
    QColor taskChecked;
    QColor taskUnchecked;
    QColor tableBorder;
    QColor footnote;
    QColor strikethrough;
    QColor comment;
};

class MarkdownHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit MarkdownHighlighter(QTextDocument* parent = nullptr);
    ~MarkdownHighlighter() override;

    /// 设置主题颜色
    void setColors(const HighlightColors& colors);

    /// 获取当前主题颜色
    const HighlightColors& colors() const { return m_colors; }

    /// 设置浅色主题默认颜色
    void applyLightTheme();

    /// 设置深色主题默认颜色
    void applyDarkTheme();

protected:
    void highlightBlock(const QString& text) override;

private:
    // ─── 块级规则 ─────────────────────────────────────────────────────────
    void highlightCodeBlock(const QString& text, int blockNumber);
    void highlightHeading(const QString& text);
    void highlightHorizontalRule(const QString& text);
    void highlightBlockquote(const QString& text);
    void highlightListItem(const QString& text);
    void highlightTable(const QString& text);
    void highlightFootnoteDef(const QString& text);

    // ─── 内联规则 ─────────────────────────────────────────────────────────
    void highlightInlineCode(const QString& text);
    void highlightBold(const QString& text);
    void highlightItalic(const QString& text);
    void highlightLinks(const QString& text);
    void highlightImages(const QString& text);
    void highlightStrikethrough(const QString& text);
    void highlightTaskItems(const QString& text);
    void highlightHtmlEntities(const QString& text);

    // ─── 辅助 ─────────────────────────────────────────────────────────────
    void setFormat(int start, int count, const QTextCharFormat& format);
    void applyFormat(int start, int count, const QColor& color,
                     bool bold = false, bool italic = false);

    HighlightColors m_colors;
    bool m_inCodeBlock = false;
    QString m_codeBlockFence;

    // ─── 预编译正则 ───────────────────────────────────────────────────────
    static const QRegularExpression sm_headingRe;
    static const QRegularExpression sm_hrRe;
    static const QRegularExpression sm_blockquoteRe;
    static const QRegularExpression sm_ulRe;
    static const QRegularExpression sm_olRe;
    static const QRegularExpression sm_boldRe;
    static const QRegularExpression sm_italicRe;
    static const QRegularExpression sm_codeInlineRe;
    static const QRegularExpression sm_linkRe;
    static const QRegularExpression sm_imageRe;
    static const QRegularExpression sm_strikeRe;
    static const QRegularExpression sm_taskRe;
    static const QRegularExpression sm_htmlEntityRe;
    static const QRegularExpression sm_footnoteDefRe;
    static const QRegularExpression sm_tableRe;
    static const QRegularExpression sm_fenceRe;
};

} // namespace dmc
