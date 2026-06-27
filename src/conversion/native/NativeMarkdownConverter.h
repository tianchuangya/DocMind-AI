#pragma once

#include "../Types.h"
#include <QObject>
#include <QString>

namespace dmc {
namespace conversion {

// 原生 Markdown/HTML 转换器 - 使用 Qt 内置能力
class NativeMarkdownConverter : public QObject {
    Q_OBJECT
public:
    explicit NativeMarkdownConverter(QObject* parent = nullptr);
    ~NativeMarkdownConverter() override;

    // Markdown → HTML
    QString markdownToHtml(const QString& markdown) const;

    // HTML → Markdown
    QString htmlToMarkdown(const QString& html) const;

    // Markdown → 纯文本
    QString markdownToPlainText(const QString& markdown) const;

    // 导出 Markdown 到 HTML 文件
    TaskOutput exportToHtml(const TaskInput& input);

    // 从 HTML 文件导入为 Markdown
    TaskOutput importFromHtml(const TaskInput& input);

    // 从 Markdown 文件读取
    TaskOutput importFromMarkdown(const TaskInput& input);

private:
    // Markdown 行内格式转 HTML
    QString processInlineMarkdown(const QString& text) const;

    // HTML 标签转 Markdown
    QString convertHtmlTagToMarkdown(const QString& tag, const QString& content) const;

    // 处理 HTML 实体
    QString decodeHtmlEntities(const QString& text) const;
};

} // namespace conversion
} // namespace dmc
