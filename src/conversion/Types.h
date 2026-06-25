#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QByteArray>
#include <QMetaType>
#include <optional>
#include <cstdint>

namespace dmc {
namespace conversion {

// ─── 枚举 ─────────────────────────────────────────────

enum class Format { Markdown, DOCX, HTML, PDF, Unknown };

enum class TaskStatus { Pending, Running, Completed, Failed, Cancelled };

enum class ConversionError {
    None,
    ToolMissing,
    SourceNotFound,
    UnsupportedFormat,
    CorruptFile,
    PasswordProtected,
    ScannedPdfNoOcr,
    Timeout,
    Cancelled,
    Unknown,
};

enum class ConversionDirection {
    MD_to_DOCX, DOCX_to_MD,
    MD_to_HTML, HTML_to_MD,
    MD_to_PDF,  PDF_to_MD,
    Unknown
};

using TaskHandle = uint64_t;

// ─── 字符串工具 ─────────────────────────────────────────

QString errorToString(ConversionError e);
QString errorToUserMessage(ConversionError e);
Format  formatFromString(const QString& s);
QString formatToString(Format f);
ConversionDirection directionFromFormats(Format s, Format t);

// ─── 结构化块（供知识库分块用）───────────────────────────

struct StructBlock {
    enum Type { Heading, Paragraph, ListItem, CodeBlock, TableCell, Blockquote };
    Type type{Paragraph};
    int  level{0};          // 标题级别 1-6，仅 Heading 有意义
    QString text;
    int  sourceLine{-1};    // 源文档行号
    int  sourcePage{-1};    // PDF 页码（-1 表示无页码信息）
};

// ─── 来源定位（RAG 引用必需）────────────────────────────

struct SourceSpan {
    int     page{-1};       // PDF 页码；DOCX/MD/HTML 为 -1
    int     lineStart{-1};  // 源内行号
    int     charStart{-1};  // 字符偏移（备选）
    QString anchor;         // HTML 锚点 / MD 标题 slug
};

// ─── 文本提取请求 ───────────────────────────────────────

struct TextExtractionRequest {
    QString source_path;        // 磁盘文件路径（与 source_content 二选一）
    QString source_content;     // 内存内容（优先于 path，非空时用这个）
    QString source_format;      // md / docx / pdf / html
    bool    prefer_structure{true};

    enum LayoutMode { Raw, Layout };
    LayoutMode layout_mode{Raw};
};

// ─── 文本提取结果 ───────────────────────────────────────

struct TextExtractionResult {
    QString plain_text;
    QString markdown_text;
    QVector<StructBlock> blocks;
    QVector<SourceSpan>  spans;
    QString error;
    ConversionError error_code{ConversionError::None};
    bool ok{false};
};

// ─── 转换任务输入 ───────────────────────────────────────

struct TaskInput {
    QString  source_path;
    QByteArray document_snapshot;   // 内存快照（is_memory_source=true 时使用）
    bool     is_memory_source{false};
    Format   source_format{Format::Unknown};
    Format   target_format{Format::Unknown};
    QString  output_path;
    QString  resource_path;
    bool     overwrite_existing{false};
};

// ─── 转换任务输出 ───────────────────────────────────────

struct TaskOutput {
    TaskStatus status{TaskStatus::Pending};
    QStringList logs;
    std::optional<QString> product_path;
    std::optional<QString> error_message;
    ConversionError error_code{ConversionError::None};
    int     exit_code{-1};
    qint64  duration_ms{0};
};

// ─── 转换能力 ───────────────────────────────────────────

struct ConversionCapabilities {
    bool pandoc_ok{false}, tectonic_ok{false}, poppler_ok{false};
    QString pandoc_version, tectonic_version, poppler_version;
    QString pandoc_path, tectonic_path, poppler_path;

    bool canExtract(Format f) const {
        if (f == Format::Markdown) return true;
        if (f == Format::DOCX || f == Format::HTML) return pandoc_ok;
        if (f == Format::PDF) return poppler_ok;
        return false;
    }
};

// ─── 工具状态 ───────────────────────────────────────────

struct ToolStatus {
    QString name, version, path;
    bool available{false};
    std::optional<QString> error_message;
};

// ─── 服务统计 ───────────────────────────────────────────

struct ServiceStats {
    size_t total_tasks{0};
    size_t completed_tasks{0};
    size_t failed_tasks{0};
    size_t pending_tasks{0};
    size_t running_tasks{0};
};

// ─── 进程执行结果 ───────────────────────────────────────

struct ProcResult {
    int     exit_code{0};
    QString std_out;
    QString std_err;
    bool    timed_out{false};
    qint64  duration_ms{0};
};

} // namespace conversion
} // namespace dmc

// Qt 元类型声明（信号槽 & QtConcurrent 需要）
Q_DECLARE_METATYPE(dmc::conversion::TextExtractionRequest)
Q_DECLARE_METATYPE(dmc::conversion::TextExtractionResult)
Q_DECLARE_METATYPE(dmc::conversion::ConversionError)
Q_DECLARE_METATYPE(dmc::conversion::TaskOutput)
Q_DECLARE_METATYPE(dmc::conversion::TaskInput)
