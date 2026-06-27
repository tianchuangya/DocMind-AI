#include "MarkdownParser.h"
#include <QRegularExpression>
#include <QStringList>

namespace dmc {
namespace conversion {

MarkdownParser::MarkdownParser() {}

// ─── parse ──────────────────────────────────────────────

QVector<StructBlock> MarkdownParser::parse(const QString& markdown, int page) const {
    QVector<StructBlock> blocks;
    QStringList lines = markdown.split('\n');

    bool in_code_block = false;
    QString code_content;
    int code_start_line = -1;

    QString paragraph_buf;
    int paragraph_start = -1;

    auto flush_paragraph = [&]() {
        if (!paragraph_buf.isEmpty()) {
            StructBlock b;
            b.type       = StructBlock::Paragraph;
            b.text       = paragraph_buf.trimmed();
            b.sourceLine = paragraph_start;
            b.sourcePage = page;
            blocks.append(b);
            paragraph_buf.clear();
            paragraph_start = -1;
        }
    };

    for (int i = 0; i < lines.size(); ++i) {
        const QString& raw_line = lines[i];
        QString trimmed = raw_line.trimmed();

        // 代码块边界
        if (trimmed.startsWith("```")) {
            if (in_code_block) {
                // 结束代码块
                if (!code_content.isEmpty() && code_content.endsWith('\n'))
                    code_content.chop(1);
                StructBlock b;
                b.type       = StructBlock::CodeBlock;
                b.text       = code_content;
                b.sourceLine = code_start_line;
                b.sourcePage = page;
                blocks.append(b);
                in_code_block = false;
                code_content.clear();
            } else {
                flush_paragraph();
                in_code_block  = true;
                code_start_line = i;
            }
            continue;
        }
        if (in_code_block) {
            code_content += raw_line + '\n';
            continue;
        }

        // 空行 → 段落边界
        if (trimmed.isEmpty()) {
            flush_paragraph();
            continue;
        }

        // 标题
        if (trimmed.startsWith('#')) {
            flush_paragraph();
            blocks.append(parseHeading(trimmed, i, page));
            continue;
        }

        // 引用
        if (trimmed.startsWith("> ") || trimmed == ">") {
            flush_paragraph();
            StructBlock b;
            b.type       = StructBlock::Blockquote;
            b.text       = (trimmed.size() > 2) ? trimmed.mid(2) : QString();
            b.sourceLine = i;
            b.sourcePage = page;
            blocks.append(b);
            continue;
        }

        // 列表项
        if (trimmed.startsWith("- ") || trimmed.startsWith("* ") ||
            trimmed.startsWith("+ ") ||
            QRegularExpression("^\\d+\\.\\s").match(trimmed).hasMatch()) {
            flush_paragraph();
            blocks.append(parseListItem(trimmed, i, page));
            continue;
        }

        // 表格行
        if (trimmed.contains('|') && trimmed.endsWith('|')) {
            flush_paragraph();
            StructBlock b;
            b.type       = StructBlock::TableCell;
            b.text       = trimmed;
            b.sourceLine = i;
            b.sourcePage = page;
            blocks.append(b);
            continue;
        }

        // 段落续行
        if (paragraph_start < 0) paragraph_start = i;
        if (!paragraph_buf.isEmpty()) paragraph_buf += ' ';
        paragraph_buf += trimmed;
    }

    flush_paragraph();

    // 未闭合的代码块
    if (in_code_block && !code_content.isEmpty()) {
        StructBlock b;
        b.type       = StructBlock::CodeBlock;
        b.text       = code_content;
        b.sourceLine = code_start_line;
        b.sourcePage = page;
        blocks.append(b);
    }

    return blocks;
}

// ─── buildSpans ─────────────────────────────────────────

QVector<SourceSpan> MarkdownParser::buildSpans(const QVector<StructBlock>& blocks) const {
    QVector<SourceSpan> spans;
    spans.reserve(blocks.size());
    for (const auto& b : blocks) {
        SourceSpan sp;
        sp.page      = b.sourcePage;
        sp.lineStart = b.sourceLine;
        if (b.type == StructBlock::Heading)
            sp.anchor = makeAnchorSlug(b.text);
        spans.append(sp);
    }
    return spans;
}

// ─── toPlainText ────────────────────────────────────────

QString MarkdownParser::toPlainText(const QString& markdown) const {
    auto blocks = parse(markdown);
    QString result;
    for (const auto& b : blocks)
        result += b.text + '\n';
    return result;
}

// ─── toHtml ─────────────────────────────────────────────

QString MarkdownParser::toHtml(const QString& markdown) const {
    QString html = "<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"UTF-8\">\n</head>\n<body>\n";
    QStringList lines = markdown.split('\n');
    bool in_paragraph   = false;
    bool in_code_block  = false;

    for (const QString& line : lines) {
        QString t = line.trimmed();

        if (t.startsWith("```")) {
            if (in_code_block) { html += "</code></pre>\n"; in_code_block = false; }
            else { if (in_paragraph) { html += "</p>\n"; in_paragraph = false; }
                   html += "<pre><code>"; in_code_block = true; }
            continue;
        }
        if (in_code_block) { html += line.toHtmlEscaped() + '\n'; continue; }

        if (t.startsWith("# ")) {
            if (in_paragraph) { html += "</p>\n"; in_paragraph = false; }
            html += "<h1>" + stripMarkdown(t.mid(2)).toHtmlEscaped() + "</h1>\n"; continue;
        }
        if (t.startsWith("## ")) {
            if (in_paragraph) { html += "</p>\n"; in_paragraph = false; }
            html += "<h2>" + stripMarkdown(t.mid(3)).toHtmlEscaped() + "</h2>\n"; continue;
        }
        if (t.startsWith("### ")) {
            if (in_paragraph) { html += "</p>\n"; in_paragraph = false; }
            html += "<h3>" + stripMarkdown(t.mid(4)).toHtmlEscaped() + "</h3>\n"; continue;
        }
        if (t.startsWith("#### ")) {
            if (in_paragraph) { html += "</p>\n"; in_paragraph = false; }
            html += "<h4>" + stripMarkdown(t.mid(5)).toHtmlEscaped() + "</h4>\n"; continue;
        }

        if (t.startsWith("- ") || t.startsWith("* ")) {
            if (in_paragraph) { html += "</p>\n"; in_paragraph = false; }
            html += "<ul><li>" + stripMarkdown(t.mid(2)).toHtmlEscaped() + "</li></ul>\n";
            continue;
        }

        if (t.isEmpty()) {
            if (in_paragraph) { html += "</p>\n"; in_paragraph = false; }
            continue;
        }

        if (!in_paragraph) { html += "<p>"; in_paragraph = true; }
        else               { html += ' '; }
        html += stripMarkdown(t).toHtmlEscaped();
    }
    if (in_paragraph) html += "</p>\n";
    html += "</body>\n</html>";
    return html;
}

// ─── 内部方法 ───────────────────────────────────────────

StructBlock MarkdownParser::parseHeading(const QString& line, int line_num, int page) const {
    StructBlock block;
    block.type       = StructBlock::Heading;
    block.sourceLine = line_num;
    block.sourcePage = page;

    int level = 0, i = 0;
    while (i < line.size() && line[i] == '#') { ++level; ++i; }
    block.level = qMin(level, 6);
    block.text  = stripMarkdown(line.mid(i).trimmed());
    return block;
}

StructBlock MarkdownParser::parseListItem(const QString& line, int line_num, int page) const {
    StructBlock block;
    block.type       = StructBlock::ListItem;
    block.sourceLine = line_num;
    block.sourcePage = page;

    QString text = line;
    if (text.startsWith("- ") || text.startsWith("* ") || text.startsWith("+ "))
        text = text.mid(2);
    else {
        QRegularExpression re("^\\d+\\.\\s");
        auto match = re.match(text);
        if (match.hasMatch()) text = text.mid(match.capturedLength());
    }
    block.text = stripMarkdown(text.trimmed());
    return block;
}

QString MarkdownParser::stripMarkdown(const QString& text) const {
    QString r = text;
    r.remove(QRegularExpression("\\*\\*([^*]+)\\*\\*"));
    r.remove(QRegularExpression("\\*([^*]+)\\*"));
    r.remove(QRegularExpression("__([^_]+)__"));
    r.remove(QRegularExpression("_([^_]+)_"));
    r.remove(QRegularExpression("\\[([^\\]]+)\\]\\([^)]+\\)"));
    r.remove(QRegularExpression("`([^`]+)`"));
    return r.trimmed();
}

QString MarkdownParser::makeAnchorSlug(const QString& heading_text) const {
    QString slug;
    for (const QChar& c : heading_text) {
        if (c.isLetterOrNumber()) slug += c.toLower();
        else if (c == ' ')        slug += '-';
    }
    return slug;
}

} // namespace conversion
} // namespace dmc
