#pragma once

#include <QObject>
#include <QString>
#include <QFuture>
#include <QVector>
#include <QHash>
#include <QMutex>
#include <QAtomicInt>
#include <QElapsedTimer>
#include <QThread>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QRegularExpression>
#include <QtConcurrent>
#include <QSharedPointer>

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <condition_variable>
#include <queue>
#include <thread>
#include <cstdint>

namespace dmc {
namespace conversion {

enum class Format { Markdown, DOCX, HTML, PDF, Unknown };
enum class TaskStatus { Pending, Running, Completed, Failed, Cancelled };
using TaskHandle = uint64_t;

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

QString errorToString(ConversionError e);
QString errorToUserMessage(ConversionError e);

enum class ConversionDirection {
    MD_to_DOCX, DOCX_to_MD, MD_to_HTML, HTML_to_MD, MD_to_PDF, PDF_to_MD, Unknown
};

struct StructBlock {
    enum Type { Heading, Paragraph, ListItem, CodeBlock, TableCell, Blockquote };
    Type type{Paragraph};
    int level{0};
    QString text;
    int sourceLine{-1};
    int sourcePage{-1};
};

struct SourceSpan {
    int page{-1};
    int lineStart{-1};
    int charStart{-1};
    QString anchor;
};

struct TextExtractionRequest {
    QString source_path;
    QString source_content;
    QString source_format;
    bool prefer_structure{true};
    enum LayoutMode { Raw, Layout };
    LayoutMode layout_mode{Raw};
};

struct TextExtractionResult {
    QString plain_text;
    QString markdown_text;
    QVector<StructBlock> blocks;
    QVector<SourceSpan> spans;
    QString error;
    ConversionError error_code{ConversionError::None};
    bool ok{false};
};

struct ConversionCapabilities {
    bool pandoc_ok{false};
    bool tectonic_ok{false};
    bool poppler_ok{false};
    QString pandoc_version;
    QString tectonic_version;
    QString poppler_version;
    QString pandoc_path;
    QString tectonic_path;
    QString poppler_path;
    bool canExtract(Format f) const {
        if (f == Format::Markdown) return true;
        if (f == Format::DOCX || f == Format::HTML) return pandoc_ok;
        if (f == Format::PDF) return poppler_ok;
        return false;
    }
};

struct TaskInput {
    QString source_path;
    QByteArray document_snapshot;
    bool is_memory_source{false};
    Format source_format{Format::Unknown};
    Format target_format{Format::Unknown};
    QString output_path;
    QString resource_path;
    bool overwrite_existing{false};
};

struct TaskOutput {
    TaskStatus status{TaskStatus::Pending};
    QStringList logs;
    std::optional<QString> product_path;
    std::optional<QString> error_message;
    ConversionError error_code{ConversionError::None};
    int exit_code{-1};
    qint64 duration_ms{0};
};

struct ToolStatus {
    QString name;
    QString version;
    QString path;
    bool available{false};
    std::optional<QString> error_message;
};

struct ServiceStats {
    size_t total_tasks{0};
    size_t completed_tasks{0};
    size_t failed_tasks{0};
    size_t pending_tasks{0};
    size_t running_tasks{0};
};

Format formatFromString(const QString& s);
QString formatToString(Format f);
ConversionDirection directionFromFormats(Format s, Format t);

struct ProcResult {
    int exit_code{0};
    QString std_out;
    QString std_err;
    bool timed_out{false};
    qint64 duration_ms{0};
};

ProcResult runProcess(const QString& exe, const QStringList& args, int timeout_ms = 300000);
QString createTempDir();
void cleanupDir(const QString& dir);
bool fileExists(const QString& path);
QString readFileToString(const QString& path);
bool isPasswordProtected(const QString& stderrText);
QVector<StructBlock> parseMarkdownBlocks(const QString& md, int page = -1);
QVector<SourceSpan> buildSourceSpans(const QVector<StructBlock>& blocks);

class ToolRunner {
public:
    static std::optional<QString> findExecutable(const QString& name, const QStringList& paths = {});
    static QString bundledToolsDir();
};

class PandocConverter {
public:
    bool isAvailable() const;
    static QString executablePath();
    ProcResult mdToDocx(const QString& in, const QString& out, const QString& res = "");
    ProcResult docxToMd(const QString& in, const QString& out);
    ProcResult mdToHtml(const QString& in, const QString& out, const QString& res = "");
    ProcResult htmlToMd(const QString& in, const QString& out);
private:
    mutable std::optional<bool> avail_;
};

class TectonicConverter {
public:
    bool isAvailable() const;
    static QString executablePath();
    ProcResult mdToPdf(const QString& in, const QString& out, const QString& res = "");
private:
    mutable std::optional<bool> avail_;
};

class PopplerExtractor {
public:
    bool isAvailable() const;
    static QString executablePath();
    ProcResult pdfToMd(const QString& in, const QString& out, TextExtractionRequest::LayoutMode mode = TextExtractionRequest::Raw);
    ProcResult pdfToStdout(const QString& in, TextExtractionRequest::LayoutMode mode = TextExtractionRequest::Raw);
    bool isScannedPdf(const QString& path) const;
private:
    mutable std::optional<bool> avail_;
};

class Diagnostics {
public:
    static constexpr const char* SCANNED_PDF_WARNING =
        "PDF appears to be scanned (image-based). OCR capability required.";
    QVector<ToolStatus> checkAllTools() const;
    ToolStatus checkTool(const QString& name) const;
    ConversionCapabilities getCapabilities() const;
};

class ResourceManager {
public:
    static QString createTempDir();
    void cleanupTempDir(const QString& dir);
    bool confirmOverwrite(const QString& path) const;
    bool copyFile(const QString& from, const QString& to) const;
    QString resolveRelativePath(const QString& base, const QString& rel) const;
};

class ConversionEngine : public QObject {
    Q_OBJECT
public:
    explicit ConversionEngine(QObject* parent = nullptr);
    ~ConversionEngine();

    TaskHandle convert(const TaskInput& in);
    QVector<TaskHandle> convertBatch(const QVector<TaskInput>& inputs);

    TaskOutput getStatus(TaskHandle h);
    bool cancel(TaskHandle h);
    void cancelAll();
    void wait(TaskHandle h);
    void waitAll();

    ServiceStats getStats() const;
    bool isToolAvailable(const QString& name) const;
    QVector<ConversionDirection> getSupportedDirections() const;
    bool canConvert(Format s, Format t) const;
    ConversionCapabilities capabilities() const;

    TextExtractionResult extractText(const TextExtractionRequest& req);
    void extractTextAsync(const TextExtractionRequest& req);
    void extractTextBatch(const QVector<TextExtractionRequest>& reqs);
    void cancelBatch();

signals:
    void extractionFinished(const TextExtractionRequest& req,
                             const TextExtractionResult& result);
    void extractionFailed(const TextExtractionRequest& req,
                           ConversionError code,
                           const QString& message);
    void batchProgress(int completed, int total, const QString& currentFile);
    void batchFinished();
    void taskFinished(TaskHandle handle, const TaskOutput& output);

private:
    void processTask(TaskHandle handle);

    class TaskQueue;
    std::unique_ptr<TaskQueue> queue_;
    mutable std::shared_mutex mu_;
    Diagnostics diag_;
    ResourceManager rm_;
    mutable std::atomic<size_t> total_{0};
    mutable std::atomic<size_t> done_{0};
    mutable std::atomic<size_t> fail_{0};
    std::atomic<bool> batchCancelled_{false};
    std::atomic<int> batchTotal_{0};
    std::atomic<int> batchCompleted_{0};
};

} // namespace conversion
} // namespace dmc

Q_DECLARE_METATYPE(dmc::conversion::TextExtractionRequest)
Q_DECLARE_METATYPE(dmc::conversion::TextExtractionResult)
Q_DECLARE_METATYPE(dmc::conversion::ConversionError)
Q_DECLARE_METATYPE(dmc::conversion::TaskOutput)
