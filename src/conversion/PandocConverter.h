#pragma once

#include "Types.h"
#include "ProcessRunner.h"
#include <QObject>

namespace dmc {
namespace conversion {

/// Pandoc 转换器 — MD↔DOCX, MD↔HTML 转换 & DOCX/HTML 文本提取
class PandocConverter : public QObject {
    Q_OBJECT
public:
    explicit PandocConverter(QObject* parent = nullptr);
    ~PandocConverter() override;

    void    setPandocPath(const QString& p) { m_pandoc_path = p; }
    QString pandocPath() const { return m_pandoc_path; }
    bool    isAvailable() const;
    QString version() const;

    /// 文件→文件转换
    TaskOutput convert(const TaskInput& input);

    /// 从 DOCX 提取文本（内存结果，不落盘）
    TextExtractionResult extractFromDocx(const QString& docx_path);

    /// 从 HTML 提取文本（内存结果，不落盘）
    TextExtractionResult extractFromHtml(const QString& html_path);

    /// 从内存内容提取（source_content 直接转 MD 结构）
    TextExtractionResult extractFromContent(const QString& content, const QString& format_hint);

    void cancel();

signals:
    void progress(int percent);
    void logMessage(const QString& message);

private:
    QString m_pandoc_path;
    ProcessRunner* m_runner{nullptr};
    bool m_cancelled{false};

    QStringList buildArgs(const TaskInput& input) const;
};

} // namespace conversion
} // namespace dmc
