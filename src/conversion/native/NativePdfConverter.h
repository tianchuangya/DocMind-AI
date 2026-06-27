#pragma once

#include "../Types.h"
#include <QObject>
#include <QString>
#include <QPageSize>

namespace dmc {
namespace conversion {

// 原生 PDF 转换器 - 使用 Qt PDF 模块
class NativePdfConverter : public QObject {
    Q_OBJECT
public:
    explicit NativePdfConverter(QObject* parent = nullptr);
    ~NativePdfConverter() override;

    // Markdown → PDF
    TaskOutput exportToPdf(const TaskInput& input);

    // PDF → Markdown
    TaskOutput importFromPdf(const TaskInput& input);

    // 设置页面参数
    void setPageSize(QPageSize::PageSizeId size) { m_page_size = size; }
    void setMargins(int top, int right, int bottom, int left);

private:
    QPageSize::PageSizeId m_page_size{QPageSize::A4};
    int m_margin_top{72};    // 1 inch = 72 points
    int m_margin_right{72};
    int m_margin_bottom{72};
    int m_margin_left{72};

    // Markdown → HTML（用于 QTextDocument 渲染）
    QString markdownToHtml(const QString& markdown) const;

    // 处理行内 Markdown 格式
    QString processInlineMarkdown(const QString& text) const;
};

} // namespace conversion
} // namespace dmc
