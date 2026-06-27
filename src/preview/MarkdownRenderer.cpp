// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — MarkdownRenderer 实现
// ─────────────────────────────────────────────────────────────────────────────
#include "preview/MarkdownRenderer.h"

#include <QUrl>
#include <QCryptographicHash>
#include <algorithm>

namespace dmc {

// ─── 预编译正则 ──────────────────────────────────────────────────────────────
const QRegularExpression MarkdownRenderer::sm_headingRe(
    R"(^(#{1,6})\s+(.+?)(?:\s+#+\s*)?$)");
const QRegularExpression MarkdownRenderer::sm_hrRe(
    R"(^(\*{3,}|-{3,}|_{3,})\s*$)");
const QRegularExpression MarkdownRenderer::sm_ulItemRe(
    R"(^(\s*)([*+\-])\s+(.+)$)");
const QRegularExpression MarkdownRenderer::sm_olItemRe(
    R"(^(\s*)(\d+)[.)]\s+(.+)$)");
const QRegularExpression MarkdownRenderer::sm_tableAlignRe(
    R"(^\|?\s*:?-{2,}:?\s*(\|\s*:?-{2,}:?\s*)*\|?\s*$)");
const QRegularExpression MarkdownRenderer::sm_boldRe(
    R"(\*\*(.+?)\*\*|__(.+?)__)");
const QRegularExpression MarkdownRenderer::sm_italicRe(
    R"((?<!\*)\*(?!\*)(.+?)(?<!\*)\*(?!\*)|(?<!_)_(?!_)(.+?)(?<!_)_(?!_))");
const QRegularExpression MarkdownRenderer::sm_codeInlineRe(
    R"(```?)([^`]+?)\1)");
const QRegularExpression MarkdownRenderer::sm_linkRe(
    R"xx(\[([^\]]+)\]\(([^)\s]+)(?:\s+"([^"]*)")?\))xx");
const QRegularExpression MarkdownRenderer::sm_imageRe(
    R"xx(!\[([^\]]*)\]\(([^)\s]+)(?:\s+"([^"]*)")?\))xx");
const QRegularExpression MarkdownRenderer::sm_strikeRe(
    R"(~~(.+?)~~)");
const QRegularExpression MarkdownRenderer::sm_autoLinkRe(
    R"((?:^|(?<=\s))((?:https?|ftp)://[^\s<>]+))");
const QRegularExpression MarkdownRenderer::sm_footnoteRefRe(
    R"(\[\^([^\]]+)\])");
const QRegularExpression MarkdownRenderer::sm_footnoteDefRe(
    R"(^\[\^([^\]]+)\]:\s*(.+)$)");
const QRegularExpression MarkdownRenderer::sm_taskItemRe(
    R"(^\[([ xX])\]\s+)");
const QRegularExpression MarkdownRenderer::sm_lineBreakRe(
    R"(  +\n)");

// ─── 构造 / 析构 ─────────────────────────────────────────────────────────────

MarkdownRenderer::MarkdownRenderer() = default;
MarkdownRenderer::~MarkdownRenderer() = default;

// ─── 主渲染入口 ──────────────────────────────────────────────────────────────

QString MarkdownRenderer::render(const QString& markdown, const RenderOptions& opts) {
    m_opts = opts;
    m_footnotes.clear();
    m_footnoteCounter = 0;

    // 第一遍：提取脚注定义
    if (opts.footnotes) {
        QStringList lines = markdown.split('\n');
        for (const QString& line : lines) {
            auto match = sm_footnoteDefRe.match(line);
            if (match.hasMatch()) {
                m_footnotes[match.captured(1)] = match.captured(2);
            }
        }
    }

    // 第二遍：块级解析
    QString html = processBlocks(markdown);

    // 第三遍：添加脚注区域
    if (opts.footnotes && !m_footnotes.isEmpty()) {
        html += "\n<section class=\"footnotes\">\n<hr>\n<ol>\n";
        for (auto it = m_footnotes.begin(); it != m_footnotes.end(); ++it) {
            html += QString("  <li id=\"fn-%1\"><p>%2 "
                            "<a href=\"#fnref-%1\" class=\"footnote-backref\">↩</a></p></li>\n")
                        .arg(escapeAttribute(it.key()), processInline(it.value()));
        }
        html += "</ol>\n</section>\n";
    }

    return html;
}

QString MarkdownRenderer::renderInline(const QString& markdown) {
    return processInline(markdown);
}

// ─── 块级解析 ────────────────────────────────────────────────────────────────

QString MarkdownRenderer::processBlocks(const QString& text) {
    QStringList lines = text.split('\n');
    QString result;
    int i = 0;
    int n = lines.size();

    while (i < n) {
        QString line = lines[i];
        QString trimmed = line.trimmed();

        // 空行跳过
        if (trimmed.isEmpty()) {
            i++;
            continue;
        }

        // 脚注定义行（跳过，已在第一遍处理）
        if (sm_footnoteDefRe.match(trimmed).hasMatch()) {
            i++;
            continue;
        }

        // 围栏代码块
        if (trimmed.startsWith("```") || trimmed.startsWith("~~~")) {
            QChar fenceChar = trimmed.at(0);
            result += processFencedCodeBlock(lines, i, QString(3, fenceChar));
            continue;
        }

        // 标题
        auto headingMatch = sm_headingRe.match(trimmed);
        if (headingMatch.hasMatch()) {
            result += processHeading(trimmed);
            i++;
            continue;
        }

        // 水平线
        if (isHorizontalRule(trimmed)) {
            result += processHorizontalRule(trimmed);
            i++;
            continue;
        }

        // 表格（需要下一行是分隔行）
        if (i + 1 < n && trimmed.contains('|') && isTableSeparator(lines[i + 1].trimmed())) {
            result += processTable(lines, i);
            continue;
        }

        // 引用块
        if (trimmed.startsWith('>')) {
            result += processBlockquote(lines, i);
            continue;
        }

        // 无序列表
        if (sm_ulItemRe.match(line).hasMatch()) {
            result += processList(lines, i, false);
            continue;
        }

        // 有序列表
        if (sm_olItemRe.match(line).hasMatch()) {
            result += processList(lines, i, true);
            continue;
        }

        // 缩进代码块（4个空格或1个tab）
        if (line.startsWith("    ") || line.startsWith("\t")) {
            result += processIndentedCodeBlock(lines, i);
            continue;
        }

        // 段落（收集连续非空行）
        QStringList paraLines;
        while (i < n) {
            QString pl = lines[i];
            QString pt = pl.trimmed();
            if (pt.isEmpty()) break;
            if (pt.startsWith("```") || pt.startsWith("~~~")) break;
            if (sm_headingRe.match(pt).hasMatch()) break;
            if (isHorizontalRule(pt)) break;
            if (pt.startsWith('>')) break;
            if (sm_ulItemRe.match(pl).hasMatch()) break;
            if (sm_olItemRe.match(pl).hasMatch()) break;
            if (sm_footnoteDefRe.match(pt).hasMatch()) break;
            paraLines.append(pl);
            i++;
        }
        if (!paraLines.isEmpty()) {
            result += processParagraph(paraLines.join('\n'));
        }
    }

    return result;
}

QString MarkdownRenderer::processFencedCodeBlock(const QStringList& lines, int& index,
                                                   const QString& fence) {
    QString firstLine = lines[index].trimmed();
    QString lang = firstLine.mid(fence.length()).trimmed();
    // 去除反引号或波浪号
    lang = lang.replace(QRegularExpression(R"([`~])"), "").trimmed();

    index++;
    QStringList codeLines;
    int n = lines.size();

    while (index < n) {
        QString line = lines[index];
        QString trimmed = line.trimmed();
        if (trimmed.startsWith(fence) && trimmed.length() >= fence.length()) {
            // 检查是否只有 fence 字符
            bool isEnd = true;
            for (QChar c : trimmed) {
                if (fence.at(0) != c && c != ' ' && c != '\t') {
                    isEnd = false;
                    break;
                }
            }
            if (isEnd && trimmed.length() >= fence.length()) {
                index++;
                break;
            }
        }
        codeLines.append(line);
        index++;
    }

    QString code = escapeHtml(codeLines.join('\n'));
    QString langAttr = lang.isEmpty() ? "" : QString(" class=\"language-%1\"").arg(escapeAttribute(lang));
    QString langLabel = lang.isEmpty() ? "" : QString("<span class=\"code-lang\">%1</span>").arg(escapeHtml(lang));

    return QString("<div class=\"code-block\">"
                   "%1"
                   "<pre><code%2>%3</code></pre>"
                   "</div>\n")
               .arg(langLabel, langAttr, code);
}

QString MarkdownRenderer::processIndentedCodeBlock(const QStringList& lines, int& index) {
    QStringList codeLines;
    int n = lines.size();

    while (index < n) {
        QString line = lines[index];
        if (line.startsWith("    ")) {
            codeLines.append(line.mid(4));
            index++;
        } else if (line.startsWith("\t")) {
            codeLines.append(line.mid(1));
            index++;
        } else if (line.trimmed().isEmpty()) {
            codeLines.append("");
            index++;
        } else {
            break;
        }
    }

    // 去除末尾空行
    while (!codeLines.isEmpty() && codeLines.last().trimmed().isEmpty()) {
        codeLines.removeLast();
    }

    QString code = escapeHtml(codeLines.join('\n'));
    return QString("<div class=\"code-block\">"
                   "<pre><code>%1</code></pre>"
                   "</div>\n").arg(code);
}

QString MarkdownRenderer::processTable(const QStringList& lines, int& index) {
    // 表头
    QString headerLine = lines[index].trimmed();
    index++;
    // 分隔行
    QString separatorLine = lines[index].trimmed();
    index++;

    // 解析对齐方式
    QStringList sepCells = separatorLine.split('|');
    // 去除首尾空元素
    if (!sepCells.isEmpty() && sepCells.first().trimmed().isEmpty()) sepCells.removeFirst();
    if (!sepCells.isEmpty() && sepCells.last().trimmed().isEmpty()) sepCells.removeLast();

    QStringList alignments;
    for (const QString& cell : sepCells) {
        QString c = cell.trimmed();
        if (c.startsWith(':') && c.endsWith(':')) {
            alignments.append("center");
        } else if (c.endsWith(':')) {
            alignments.append("right");
        } else {
            alignments.append("left");
        }
    }

    // 构建 HTML 表格
    QString html = "<div class=\"table-wrapper\">\n<table>\n<thead>\n<tr>\n";

    // 表头
    QStringList headerCells = headerLine.split('|');
    if (!headerCells.isEmpty() && headerCells.first().trimmed().isEmpty()) headerCells.removeFirst();
    if (!headerCells.isEmpty() && headerCells.last().trimmed().isEmpty()) headerCells.removeLast();

    for (int col = 0; col < headerCells.size() && col < alignments.size(); col++) {
        QString align = alignments[col];
        QString style = (align != "left") ? QString(" style=\"text-align:%1\"").arg(align) : "";
        html += QString("<th%1>%2</th>\n")
                    .arg(style, processInline(headerCells[col].trimmed()));
    }
    html += "</tr>\n</thead>\n<tbody>\n";

    // 表体
    int n = lines.size();
    while (index < n) {
        QString line = lines[index].trimmed();
        if (line.isEmpty() || !line.contains('|')) break;

        QStringList cells = line.split('|');
        if (!cells.isEmpty() && cells.first().trimmed().isEmpty()) cells.removeFirst();
        if (!cells.isEmpty() && cells.last().trimmed().isEmpty()) cells.removeLast();

        html += "<tr>\n";
        for (int col = 0; col < cells.size() && col < alignments.size(); col++) {
            QString align = alignments[col];
            QString style = (align != "left") ? QString(" style=\"text-align:%1\"").arg(align) : "";
            html += QString("<td%1>%2</td>\n")
                        .arg(style, processInline(cells[col].trimmed()));
        }
        html += "</tr>\n";
        index++;
    }

    html += "</tbody>\n</table>\n</div>\n";
    return html;
}

QString MarkdownRenderer::processBlockquote(const QStringList& lines, int& index) {
    QStringList quoteLines;
    int n = lines.size();

    while (index < n) {
        QString line = lines[index];
        QString trimmed = line.trimmed();
        if (trimmed.startsWith('>')) {
            // 去掉 > 前缀
            QString content = trimmed.mid(1);
            if (content.startsWith(' ')) content = content.mid(1);
            quoteLines.append(content);
            index++;
        } else if (trimmed.isEmpty()) {
            break;
        } else if (!quoteLines.isEmpty()) {
            // 紧凑引用：无 > 前缀但上一行是引用
            quoteLines.append(trimmed);
            index++;
        } else {
            break;
        }
    }

    QString inner = processBlocks(quoteLines.join('\n'));
    return QString("<blockquote>\n%1</blockquote>\n").arg(inner);
}

QString MarkdownRenderer::processList(const QStringList& lines, int& index, bool ordered) {
    const QRegularExpression& itemRe = ordered ? sm_olItemRe : sm_ulItemRe;

    QString tag = ordered ? "ol" : "ul";
    QString html = QString("<%1>\n").arg(tag);

    int n = lines.size();
    int baseIndent = -1;

    while (index < n) {
        QString line = lines[index];
        auto match = itemRe.match(line);

        if (match.hasMatch()) {
            int indent = match.captured(1).length();
            if (baseIndent < 0) baseIndent = indent;

            QString content = match.captured(3);

            // 检查任务列表
            QString taskHtml;
            if (m_opts.taskLists) {
                auto taskMatch = sm_taskItemRe.match(content);
                if (taskMatch.hasMatch()) {
                    bool checked = (taskMatch.captured(1) == "x" || taskMatch.captured(1) == "X");
                    QString checkedAttr = checked ? " checked disabled" : " disabled";
                    taskHtml = QString("<input type=\"checkbox\"%1> ").arg(checkedAttr);
                    content = content.mid(taskMatch.capturedLength());
                }
            }

            html += QString("<li>%1%2</li>\n").arg(taskHtml, processInline(content));
            index++;
        } else if (line.trimmed().isEmpty()) {
            index++;
        } else {
            break;
        }
    }

    html += QString("</%1>\n").arg(tag);
    return html;
}

QString MarkdownRenderer::processHeading(const QString& line) {
    auto match = sm_headingRe.match(line);
    if (!match.hasMatch()) return "";

    int level = match.captured(1).length();
    QString text = match.captured(2).trimmed();
    QString id = generateHeadingId(text);

    return QString("<h%1 id=\"%2\">%3</h%1>\n")
               .arg(level)
               .arg(id, processInline(text));
}

QString MarkdownRenderer::processParagraph(const QString& text) {
    QString processed = processInline(text);
    return QString("<p>%1</p>\n").arg(processed);
}

QString MarkdownRenderer::processHorizontalRule(const QString& /*line*/) {
    return "<hr>\n";
}

// ─── 内联解析 ────────────────────────────────────────────────────────────────

QString MarkdownRenderer::processInline(const QString& text) {
    QString result = text;

    // 处理顺序很重要：先处理代码，防止其他格式进入代码块
    // 1. 行内代码
    result = processCodeInline(result);
    // 2. 图片（必须在链接之前，因为图片语法包含链接语法）
    result = processImages(result);
    // 3. 链接
    result = processLinks(result);
    // 4. 粗体
    result = processEmphasis(result);
    // 5. 删除线
    if (m_opts.strikethrough) {
        result = processStrikethrough(result);
    }
    // 6. 自动链接
    if (m_opts.autoLink) {
        result = processAutoLinks(result);
    }
    // 7. 脚注引用
    if (m_opts.footnotes) {
        result = processFootnotes(result);
    }
    // 8. 换行
    result = processLineBreaks(result);

    return result;
}

QString MarkdownRenderer::processCodeInline(const QString& text) {
    // 处理行内代码 `code`
    QString result;
    int pos = 0;
    int n = text.length();

    while (pos < n) {
        // 找 ` 开头
        int start = text.indexOf('`', pos);
        if (start < 0) {
            result += text.mid(pos);
            break;
        }

        result += text.mid(pos, start - pos);

        // 计算连续反引号数量
        int fenceLen = 1;
        while (start + fenceLen < n && text[start + fenceLen] == '`') fenceLen++;

        // 找匹配的结束反引号
        int end = text.indexOf(QString(fenceLen, '`'), start + fenceLen);
        if (end < 0) {
            // 未匹配，原样输出
            result += text.mid(start, fenceLen);
            pos = start + fenceLen;
        } else {
            QString code = text.mid(start + fenceLen, end - start - fenceLen);
            result += QString("<code>%1</code>").arg(escapeHtml(code));
            pos = end + fenceLen;
        }
    }

    return result;
}

QString MarkdownRenderer::processEmphasis(const QString& text) {
    QString result = text;

    // 粗体 **text** 或 __text__（使用非贪婪匹配）
    result.replace(QRegularExpression(R"(\*\*(.+?)\*\*)"), "<strong>\\1</strong>");
    // 对于 __ 需要更精确的匹配，避免匹配到单词中间的下划线
    result.replace(QRegularExpression(R"((?<![a-zA-Z0-9])__([^_]+?)__(?![a-zA-Z0-9]))"), "<strong>\\1</strong>");

    // 斜体 *text* 或 _text_（确保不是粗体的一部分）
    result.replace(QRegularExpression(R"((?<!\*)\*(?!\*)(.+?)(?<!\*)\*(?!\*))"), "<em>\\1</em>");
    result.replace(QRegularExpression(R"((?<![a-zA-Z0-9*_])_(?!_)(.+?)(?<!_)_(?![a-zA-Z0-9*_]))"), "<em>\\1</em>");

    return result;
}

QString MarkdownRenderer::processLinks(const QString& text) {
    // 先处理图片（避免冲突）——图片已在 processImages 中处理
    // 这里只处理 [text](url) 链接
    QString result = text;
    return result.replace(sm_linkRe, "<a href=\"\\2\" title=\"\\3\">\\1</a>");
}

QString MarkdownRenderer::processImages(const QString& text) {
    QString result = text;
    // ![alt](url "title")
    auto it = sm_imageRe.globalMatch(result);
    QString output;
    int lastEnd = 0;

    while (it.hasNext()) {
        auto match = it.next();
        output += result.mid(lastEnd, match.capturedStart() - lastEnd);

        QString alt = match.captured(1);
        QString url = match.captured(2);
        QString title = match.captured(3);

        // 处理相对路径
        if (!m_opts.baseUrl.isEmpty() && !url.startsWith("http") && !url.startsWith("/")) {
            url = m_opts.baseUrl + "/" + url;
        }

        output += QString("<img src=\"%1\" alt=\"%2\"%3>")
                      .arg(escapeAttribute(url), escapeAttribute(alt),
                           title.isEmpty() ? "" : QString(" title=\"%1\"").arg(escapeAttribute(title)));
        lastEnd = match.capturedEnd();
    }
    output += result.mid(lastEnd);

    return output;
}

QString MarkdownRenderer::processAutoLinks(const QString& text) {
    // 自动将 URL 转换为链接（不处理已在 <a> 标签中的）
    QString result = text;
    return result.replace(sm_autoLinkRe, "<a href=\"\\2\">\\2</a>");
}

QString MarkdownRenderer::processStrikethrough(const QString& text) {
    QString result = text;
    return result.replace(sm_strikeRe, "<del>\\1</del>");
}

QString MarkdownRenderer::processLineBreaks(const QString& text) {
    // 两空格+换行 → <br>
    QString result = text;
    result.replace(QRegularExpression(R"(  +\n)"), "<br>\n");
    // 普通换行保留
    result.replace('\n', ' ');
    return result;
}

QString MarkdownRenderer::processFootnotes(const QString& text) {
    QString result = text;
    return result.replace(sm_footnoteRefRe,
        "<sup class=\"footnote-ref\"><a href=\"#fn-\\1\" id=\"fnref-\\1\">[\\1]</a></sup>");
}

QString MarkdownRenderer::processTaskListItems(const QString& text) {
    return text;
}

// ─── 工具方法 ────────────────────────────────────────────────────────────────

QString MarkdownRenderer::escapeHtml(const QString& text) {
    QString result = text;
    result.replace('&', "&amp;");
    result.replace('<', "&lt;");
    result.replace('>', "&gt;");
    result.replace('"', "&quot;");
    result.replace('\'', "&#39;");
    return result;
}

QString MarkdownRenderer::escapeAttribute(const QString& text) {
    return escapeHtml(text);
}

QString MarkdownRenderer::generateHeadingId(const QString& text) {
    // 移除 HTML 标签，转为小写，替换空格为 -
    QString id = text;
    id.remove(QRegularExpression(R"(<[^>]+>)"));
    id = id.toLower().trimmed();
    id.replace(QRegularExpression(R"([^\w一-鿿-])"), "-");
    id.replace(QRegularExpression(R"(-{2,})"), "-");
    id = id.replace(QRegularExpression(R"(^-|-$)"), "");
    if (id.isEmpty()) {
        // 用哈希值
        id = QString::number(qHash(text));
    }
    return id;
}

QString MarkdownRenderer::normalizeWhitespace(const QString& text) {
    return text.simplified();
}

bool MarkdownRenderer::isHorizontalRule(const QString& line) const {
    return sm_hrRe.match(line).hasMatch();
}

bool MarkdownRenderer::isTableSeparator(const QString& line) const {
    return sm_tableAlignRe.match(line).hasMatch();
}

// ─── 静态工具 ────────────────────────────────────────────────────────────────

QString MarkdownRenderer::extractTitle(const QString& markdown) {
    QStringList lines = markdown.split('\n');
    for (const QString& line : lines) {
        auto match = sm_headingRe.match(line.trimmed());
        if (match.hasMatch() && match.captured(1).length() == 1) {
            return match.captured(2).trimmed();
        }
    }
    return QString();
}

QList<MarkdownRenderer::HeadingInfo> MarkdownRenderer::extractHeadings(const QString& markdown) {
    QList<HeadingInfo> headings;
    QStringList lines = markdown.split('\n');
    bool inCodeBlock = false;

    for (const QString& line : lines) {
        QString trimmed = line.trimmed();

        if (trimmed.startsWith("```") || trimmed.startsWith("~~~")) {
            inCodeBlock = !inCodeBlock;
            continue;
        }
        if (inCodeBlock) continue;

        auto match = sm_headingRe.match(trimmed);
        if (match.hasMatch()) {
            HeadingInfo info;
            info.level = match.captured(1).length();
            info.text  = match.captured(2).trimmed();
            info.id    = generateHeadingId(info.text);
            headings.append(info);
        }
    }

    return headings;
}

} // namespace dmc
