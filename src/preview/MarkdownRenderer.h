// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — MarkdownRenderer
// Markdown → HTML 渲染引擎（支持表格、代码块、任务列表、脚注等）
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <QString>
#include <QStringList>
#include <QMap>
#include <QRegularExpression>
#include <functional>

namespace dmc {

/// 渲染配置
struct RenderOptions {
    bool tables         = true;    // 表格
    bool taskLists      = true;    // 任务列表 [x]
    bool strikethrough  = true;    // ~~删除线~~
    bool autoLink       = true;    // 自动链接
    bool footnotes      = true;    // 脚注 [^1]
    bool codeHighlight  = true;    // 代码块语言标注
    bool mathSupport    = false;   // 数学公式（预留）
    QString baseUrl;               // 图片相对路径基准
};

/// Markdown → HTML 渲染器
class MarkdownRenderer {
public:
    MarkdownRenderer();
    ~MarkdownRenderer();

    /// 渲染完整 Markdown 文档为 HTML
    QString render(const QString& markdown, const RenderOptions& opts = {});

    /// 渲染单行内联格式（用于预览标题等场景）
    QString renderInline(const QString& markdown);

    /// 设置选项
    void setOptions(const RenderOptions& opts) { m_opts = opts; }
    RenderOptions options() const { return m_opts; }

    /// 提取文档标题（第一个 # 标题）
    static QString extractTitle(const QString& markdown);

    /// 提取所有标题（用于目录生成）
    struct HeadingInfo {
        int level;
        QString text;
        QString id;
    };
    static QList<HeadingInfo> extractHeadings(const QString& markdown);

private:
    // ─── 块级解析 ─────────────────────────────────────────────────────────
    QString processBlocks(const QString& text);
    QString processFencedCodeBlock(const QStringList& lines, int& index, const QString& fence);
    QString processIndentedCodeBlock(const QStringList& lines, int& index);
    QString processTable(const QStringList& lines, int& index);
    QString processBlockquote(const QStringList& lines, int& index);
    QString processList(const QStringList& lines, int& index, bool ordered);
    QString processHeading(const QString& line);
    QString processParagraph(const QString& text);
    QString processHorizontalRule(const QString& line);

    // ─── 内联解析 ─────────────────────────────────────────────────────────
    QString processInline(const QString& text);
    QString processCodeInline(const QString& text);
    QString processEmphasis(const QString& text);
    QString processLinks(const QString& text);
    QString processImages(const QString& text);
    QString processAutoLinks(const QString& text);
    QString processStrikethrough(const QString& text);
    QString processLineBreaks(const QString& text);
    QString processFootnotes(const QString& text);
    QString processTaskListItems(const QString& text);

    // ─── 工具方法 ─────────────────────────────────────────────────────────
    static QString escapeHtml(const QString& text);
    static QString escapeAttribute(const QString& text);
    static QString generateHeadingId(const QString& text);
    static QString normalizeWhitespace(const QString& text);
    bool isHorizontalRule(const QString& line) const;
    bool isTableSeparator(const QString& line) const;

    // ─── 脚注存储 ─────────────────────────────────────────────────────────
    QMap<QString, QString> m_footnotes;
    int m_footnoteCounter = 0;
    RenderOptions m_opts;

    // ─── 正则表达式（预编译） ─────────────────────────────────────────────
    static const QRegularExpression sm_headingRe;
    static const QRegularExpression sm_hrRe;
    static const QRegularExpression sm_ulItemRe;
    static const QRegularExpression sm_olItemRe;
    static const QRegularExpression sm_tableAlignRe;
    static const QRegularExpression sm_boldRe;
    static const QRegularExpression sm_italicRe;
    static const QRegularExpression sm_codeInlineRe;
    static const QRegularExpression sm_linkRe;
    static const QRegularExpression sm_imageRe;
    static const QRegularExpression sm_strikeRe;
    static const QRegularExpression sm_autoLinkRe;
    static const QRegularExpression sm_footnoteRefRe;
    static const QRegularExpression sm_footnoteDefRe;
    static const QRegularExpression sm_taskItemRe;
    static const QRegularExpression sm_lineBreakRe;
};

} // namespace dmc
