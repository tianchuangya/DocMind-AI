#pragma once

#include "Types.h"
#include "ProcessRunner.h"
#include "MarkdownParser.h"
#include <QObject>

namespace dmc {
namespace conversion {

/// Poppler 提取器 — 使用 pdftotext 从 PDF 提取文本，保留页码
class PopplerExtractor : public QObject {
    Q_OBJECT
public:
    explicit PopplerExtractor(QObject* parent = nullptr);
    ~PopplerExtractor() override;

    void    setPopplerPath(const QString& p) { m_poppler_path = p; }
    QString popplerPath() const { return m_poppler_path; }
    bool    isAvailable() const;
    QString version() const;

    /// 从 PDF 提取文本（内存结果，含页码追踪）
    TextExtractionResult extractText(const QString& pdf_path,
                                     bool layout_mode = false);

    /// 将提取结果写入文件
    TaskOutput extractToFile(const QString& pdf_path,
                             const QString& output_path,
                             bool layout_mode = false);

    void cancel();

signals:
    void progress(int percent);
    void logMessage(const QString& message);

private:
    QString m_poppler_path;
    ProcessRunner* m_runner{nullptr};
    bool m_cancelled{false};

    /// 按分页符拆分文本并逐页解析结构块
    void parsePagesWithTracking(const QString& raw_text,
                                TextExtractionResult& result) const;

    /// 判断是否为扫描件（基于图片覆盖率而非简单字数阈值）
    bool isScannedPdf(const QString& pdf_path) const;
};

} // namespace conversion
} // namespace dmc
