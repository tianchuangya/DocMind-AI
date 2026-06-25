#pragma once

#include "Types.h"
#include <QString>
#include <QVector>

namespace dmc {
namespace conversion {

/// Markdown 解析器 — 将 Markdown 文本解析为结构化块
class MarkdownParser {
public:
    MarkdownParser();

    /// 解析 Markdown 为结构化块（可指定页码）
    QVector<StructBlock> parse(const QString& markdown, int page = -1) const;

    /// 生成 SourceSpan 列表（含标题 anchor）
    QVector<SourceSpan> buildSpans(const QVector<StructBlock>& blocks) const;

    /// 解析 Markdown 为纯文本
    QString toPlainText(const QString& markdown) const;

    /// 解析 Markdown 为 HTML
    QString toHtml(const QString& markdown) const;

private:
    StructBlock parseHeading(const QString& line, int line_num, int page) const;
    StructBlock parseListItem(const QString& line, int line_num, int page) const;
    QString     stripMarkdown(const QString& text) const;
    QString     makeAnchorSlug(const QString& heading_text) const;
};

} // namespace conversion
} // namespace dmc
