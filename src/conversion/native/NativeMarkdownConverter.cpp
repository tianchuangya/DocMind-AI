#include "NativeMarkdownConverter.h"
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QTextDocument>
#include <QTextCursor>
#include <QDateTime>

namespace dmc {
namespace conversion {

NativeMarkdownConverter::NativeMarkdownConverter(QObject* parent) : QObject(parent) {}

NativeMarkdownConverter::~NativeMarkdownConverter() {}

QString NativeMarkdownConverter::markdownToHtml(const QString& markdown) const {
    QString html;
    QStringList lines = markdown.split('\n');
    
    bool in_code_block = false;
    bool in_paragraph = false;
    bool in_list = false;
    QString code_lang;
    
    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i];
        QString trimmed = line.trimmed();
        
        // 代码块
        if (trimmed.startsWith("```")) {
            if (in_code_block) {
                html += "</code></pre>\n";
                in_code_block = false;
            } else {
                if (in_paragraph) { html += "</p>\n"; in_paragraph = false; }
                if (in_list) { html += "</ul>\n"; in_list = false; }
                in_code_block = true;
                code_lang = trimmed.mid(3).trimmed();
                if (!code_lang.isEmpty()) {
                    html += "<pre><code class=\"language-" + code_lang + "\">";
                } else {
                    html += "<pre><code>";
                }
            }
            continue;
        }
        
        if (in_code_block) {
            html += line.toHtmlEscaped() + '\n';
            continue;
        }
        
        // 空行
        if (trimmed.isEmpty()) {
            if (in_paragraph) { html += "</p>\n"; in_paragraph = false; }
            if (in_list) { html += "</ul>\n"; in_list = false; }
            continue;
        }
        
        // 标题
        if (trimmed.startsWith("# ")) {
            if (in_paragraph) { html += "</p>\n"; in_paragraph = false; }
            if (in_list) { html += "</ul>\n"; in_list = false; }
            html += "<h1>" + processInlineMarkdown(trimmed.mid(2)) + "</h1>\n";
            continue;
        }
        if (trimmed.startsWith("## ")) {
            if (in_paragraph) { html += "</p>\n"; in_paragraph = false; }
            if (in_list) { html += "</ul>\n"; in_list = false; }
            html += "<h2>" + processInlineMarkdown(trimmed.mid(3)) + "</h2>\n";
            continue;
        }
        if (trimmed.startsWith("### ")) {
            if (in_paragraph) { html += "</p>\n"; in_paragraph = false; }
            if (in_list) { html += "</ul>\n"; in_list = false; }
            html += "<h3>" + processInlineMarkdown(trimmed.mid(4)) + "</h3>\n";
            continue;
        }
        if (trimmed.startsWith("#### ")) {
            if (in_paragraph) { html += "</p>\n"; in_paragraph = false; }
            if (in_list) { html += "</ul>\n"; in_list = false; }
            html += "<h4>" + processInlineMarkdown(trimmed.mid(5)) + "</h4>\n";
            continue;
        }
        
        // 水平线
        if (trimmed == "---" || trimmed == "***" || trimmed == "___") {
            if (in_paragraph) { html += "</p>\n"; in_paragraph = false; }
            if (in_list) { html += "</ul>\n"; in_list = false; }
            html += "<hr>\n";
            continue;
        }
        
        // 无序列表
        if (trimmed.startsWith("- ") || trimmed.startsWith("* ") || trimmed.startsWith("+ ")) {
            if (in_paragraph) { html += "</p>\n"; in_paragraph = false; }
            if (!in_list) { html += "<ul>\n"; in_list = true; }
            html += "<li>" + processInlineMarkdown(trimmed.mid(2)) + "</li>\n";
            continue;
        }
        
        // 有序列表
        QRegularExpression ol_re("^(\\d+)\\.\\s+(.+)$");
        auto ol_match = ol_re.match(trimmed);
        if (ol_match.hasMatch()) {
            if (in_paragraph) { html += "</p>\n"; in_paragraph = false; }
            if (!in_list) { html += "<ol>\n"; in_list = true; }
            html += "<li>" + processInlineMarkdown(ol_match.captured(2)) + "</li>\n";
            continue;
        }
        
        // 引用
        if (trimmed.startsWith("> ")) {
            if (in_paragraph) { html += "</p>\n"; in_paragraph = false; }
            if (in_list) { html += "</ul>\n"; in_list = false; }
            html += "<blockquote><p>" + processInlineMarkdown(trimmed.mid(2)) + "</p></blockquote>\n";
            continue;
        }
        
        // 普通段落
        if (!in_paragraph) {
            html += "<p>";
            in_paragraph = true;
        } else {
            html += " ";
        }
        html += processInlineMarkdown(trimmed);
    }
    
    if (in_paragraph) html += "</p>\n";
    if (in_list) html += "</ul>\n";
    if (in_code_block) html += "</code></pre>\n";
    
    return html;
}

QString NativeMarkdownConverter::htmlToMarkdown(const QString& html) const {
    QString markdown;
    QString content = html;
    
    // 提取 body 内容
    QRegularExpression body_re("<body[^>]*>(.*)</body>", QRegularExpression::DotMatchesEverythingOption);
    auto body_match = body_re.match(content);
    if (body_match.hasMatch()) {
        content = body_match.captured(1);
    }
    
    // 处理标题
    for (int i = 1; i <= 6; ++i) {
        QRegularExpression re("<h" + QString::number(i) + "[^>]*>(.*?)</h" + QString::number(i) + ">",
                             QRegularExpression::DotMatchesEverythingOption);
        content.replace(re, QString("\n") + QString("#").repeated(i) + " \\1\n\n");
    }
    
    // 处理段落
    content.replace(QRegularExpression("<p[^>]*>(.*?)</p>", QRegularExpression::DotMatchesEverythingOption),
                   "\\1\n\n");
    
    // 处理粗体
    content.replace(QRegularExpression("<(?:b|strong)[^>]*>(.*?)</(?:b|strong)>",
                                       QRegularExpression::DotMatchesEverythingOption),
                   "**\\1**");
    
    // 处理斜体
    content.replace(QRegularExpression("<(?:i|em)[^>]*>(.*?)</(?:i|em)>",
                                       QRegularExpression::DotMatchesEverythingOption),
                   "*\\1*");
    
    // 处理删除线
    content.replace(QRegularExpression("<(?:s|del|strike)[^>]*>(.*?)</(?:s|del|strike)>",
                                       QRegularExpression::DotMatchesEverythingOption),
                   "~~\\1~~");
    
    // 处理行内代码
    content.replace(QRegularExpression("<code[^>]*>(.*?)</code>",
                                       QRegularExpression::DotMatchesEverythingOption),
                   "`\\1`");
    
    // 处理链接
    content.replace(QRegularExpression("<a[^>]*href=\"([^\"]*)\"[^>]*>(.*?)</a>",
                                       QRegularExpression::DotMatchesEverythingOption),
                   "[\\2](\\1)");
    
    // 处理图片
    content.replace(QRegularExpression("<img[^>]*src=\"([^\"]*)\"[^>]*alt=\"([^\"]*)\"[^>]*/?>",
                                       QRegularExpression::DotMatchesEverythingOption),
                   "![\\2](\\1)");
    content.replace(QRegularExpression("<img[^>]*src=\"([^\"]*)\"[^>]*/?>",
                                       QRegularExpression::DotMatchesEverythingOption),
                   "![](\\1)");
    
    // 处理列表
    content.replace(QRegularExpression("<li[^>]*>(.*?)</li>",
                                       QRegularExpression::DotMatchesEverythingOption),
                   "- \\1\n");
    content.replace(QRegularExpression("</?[uo]l[^>]*>\n*",
                                       QRegularExpression::DotMatchesEverythingOption),
                   "\n");
    
    // 处理引用
    content.replace(QRegularExpression("<blockquote[^>]*>(.*?)</blockquote>",
                                       QRegularExpression::DotMatchesEverythingOption),
                   "> \\1\n");
    
    // 处理代码块
    content.replace(QRegularExpression("<pre[^>]*><code[^>]*>(.*?)</code></pre>",
                                       QRegularExpression::DotMatchesEverythingOption),
                   "\n```\n\\1\n```\n");
    
    // 处理水平线
    content.replace(QRegularExpression("<hr[^>]*/?>"), "\n---\n");
    
    // 处理换行
    content.replace(QRegularExpression("<br[^>]*/?>"), "\n");
    
    // 移除剩余 HTML 标签
    content.replace(QRegularExpression("<[^>]+>"), "");
    
    // 解码 HTML 实体
    content = decodeHtmlEntities(content);
    
    // 清理多余空行
    content.replace(QRegularExpression("\n{3,}"), "\n\n");
    
    return content.trimmed();
}

QString NativeMarkdownConverter::markdownToPlainText(const QString& markdown) const {
    QString text = markdown;
    
    // 移除标题标记
    text.replace(QRegularExpression("^#{1,6}\\s+", QRegularExpression::MultilineOption), "");
    
    // 移除粗体/斜体
    text.replace(QRegularExpression("\\*\\*([^*]+)\\*\\*"), "\\1");
    text.replace(QRegularExpression("\\*([^*]+)\\*"), "\\1");
    text.replace(QRegularExpression("__([^_]+)__"), "\\1");
    text.replace(QRegularExpression("_([^_]+)_"), "\\1");
    
    // 移除链接，保留文本
    text.replace(QRegularExpression("\\[([^\\]]+)\\]\\([^)]+\\)"), "\\1");
    
    // 移除图片
    text.replace(QRegularExpression("!\\[[^\\]]*\\]\\([^)]+\\)"), "");
    
    // 移除代码块
    text.replace(QRegularExpression("```[\\s\\S]*?```"), "");
    
    // 移除行内代码
    text.replace(QRegularExpression("`([^`]+)`"), "\\1");
    
    // 移除引用标记
    text.replace(QRegularExpression("^>\\s+", QRegularExpression::MultilineOption), "");
    
    // 移除列表标记
    text.replace(QRegularExpression("^[-*+]\\s+", QRegularExpression::MultilineOption), "");
    text.replace(QRegularExpression("^\\d+\\.\\s+", QRegularExpression::MultilineOption), "");
    
    return text;
}

TaskOutput NativeMarkdownConverter::exportToHtml(const TaskInput& input) {
    TaskOutput output;
    output.status = TaskStatus::Running;
    
    QElapsedTimer timer;
    timer.start();
    
    // 读取源文件
    QFile source_file(input.source_path);
    if (!source_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        output.status = TaskStatus::Failed;
        output.error_code = ConversionError::SourceNotFound;
        output.error_message = "无法打开源文件: " + input.source_path;
        return output;
    }
    
    QString markdown = QString::fromUtf8(source_file.readAll());
    source_file.close();
    
    // 转换为 HTML
    QString html_content = markdownToHtml(markdown);
    
    // 包装完整 HTML 文档
    QString full_html = R"(<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="generator" content="DocMind-AI">
    <title>)" + QFileInfo(input.source_path).baseName() + R"(</title>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; 
               max-width: 800px; margin: 0 auto; padding: 20px; line-height: 1.6; }
        h1, h2, h3, h4 { margin-top: 1.5em; margin-bottom: 0.5em; }
        code { background: #f4f4f4; padding: 2px 6px; border-radius: 3px; }
        pre { background: #f4f4f4; padding: 16px; border-radius: 6px; overflow-x: auto; }
        blockquote { border-left: 4px solid #ddd; margin: 0; padding-left: 16px; color: #666; }
    </style>
</head>
<body>
)" + html_content + R"(
</body>
</html>)";
    
    // 写入输出文件
    QFile output_file(input.output_path);
    if (!output_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        output.status = TaskStatus::Failed;
        output.error_code = ConversionError::Unknown;
        output.error_message = "无法写入输出文件: " + input.output_path;
        return output;
    }
    
    output_file.write(full_html.toUtf8());
    output_file.close();
    
    output.status = TaskStatus::Completed;
    output.product_path = input.output_path;
    output.duration_ms = timer.elapsed();
    output.logs << "Markdown → HTML 转换成功";
    
    return output;
}

TaskOutput NativeMarkdownConverter::importFromHtml(const TaskInput& input) {
    TaskOutput output;
    output.status = TaskStatus::Running;
    
    QElapsedTimer timer;
    timer.start();
    
    // 读取源文件
    QFile source_file(input.source_path);
    if (!source_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        output.status = TaskStatus::Failed;
        output.error_code = ConversionError::SourceNotFound;
        output.error_message = "无法打开源文件: " + input.source_path;
        return output;
    }
    
    QString html = QString::fromUtf8(source_file.readAll());
    source_file.close();
    
    // 转换为 Markdown
    QString markdown = htmlToMarkdown(html);
    
    // 写入输出文件
    QFile output_file(input.output_path);
    if (!output_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        output.status = TaskStatus::Failed;
        output.error_code = ConversionError::Unknown;
        output.error_message = "无法写入输出文件: " + input.output_path;
        return output;
    }
    
    output_file.write(markdown.toUtf8());
    output_file.close();
    
    output.status = TaskStatus::Completed;
    output.product_path = input.output_path;
    output.duration_ms = timer.elapsed();
    output.logs << "HTML → Markdown 转换成功";
    
    return output;
}

TaskOutput NativeMarkdownConverter::importFromMarkdown(const TaskInput& input) {
    TaskOutput output;
    output.status = TaskStatus::Running;
    
    QElapsedTimer timer;
    timer.start();
    
    // 读取源文件
    QFile source_file(input.source_path);
    if (!source_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        output.status = TaskStatus::Failed;
        output.error_code = ConversionError::SourceNotFound;
        output.error_message = "无法打开源文件: " + input.source_path;
        return output;
    }
    
    QString content = QString::fromUtf8(source_file.readAll());
    source_file.close();
    
    // 写入输出文件
    QFile output_file(input.output_path);
    if (!output_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        output.status = TaskStatus::Failed;
        output.error_code = ConversionError::Unknown;
        output.error_message = "无法写入输出文件: " + input.output_path;
        return output;
    }
    
    output_file.write(content.toUtf8());
    output_file.close();
    
    output.status = TaskStatus::Completed;
    output.product_path = input.output_path;
    output.duration_ms = timer.elapsed();
    output.logs << "Markdown 导入成功";
    
    return output;
}

QString NativeMarkdownConverter::processInlineMarkdown(const QString& text) const {
    QString result = text.toHtmlEscaped();
    
    // 行内代码
    result.replace(QRegularExpression("`([^`]+)`"), "<code>\\1</code>");
    
    // 粗体
    result.replace(QRegularExpression("\\*\\*([^*]+)\\*\\*"), "<strong>\\1</strong>");
    result.replace(QRegularExpression("__([^_]+)__"), "<strong>\\1</strong>");
    
    // 斜体
    result.replace(QRegularExpression("\\*([^*]+)\\*"), "<em>\\1</em>");
    result.replace(QRegularExpression("_([^_]+)_"), "<em>\\1</em>");
    
    // 删除线
    result.replace(QRegularExpression("~~([^~]+)~~"), "<del>\\1</del>");
    
    // 链接
    result.replace(QRegularExpression("\\[([^\\]]+)\\]\\(([^)]+)\\)"), "<a href=\"\\2\">\\1</a>");
    
    // 图片
    result.replace(QRegularExpression("!\\[([^\\]]*)\\]\\(([^)]+)\\)"), "<img src=\"\\2\" alt=\"\\1\">");
    
    return result;
}

QString NativeMarkdownConverter::convertHtmlTagToMarkdown(const QString& tag, const QString& content) const {
    Q_UNUSED(tag);
    return content;
}

QString NativeMarkdownConverter::decodeHtmlEntities(const QString& text) const {
    QString result = text;
    result.replace("&amp;", "&");
    result.replace("&lt;", "<");
    result.replace("&gt;", ">");
    result.replace("&quot;", "\"");
    result.replace("&#39;", "'");
    result.replace("&nbsp;", " ");
    return result;
}

} // namespace conversion
} // namespace dmc
