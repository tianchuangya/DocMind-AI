#include "PopplerExtractor.h"
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QDir>
#include <QElapsedTimer>

namespace dmc {
namespace conversion {

PopplerExtractor::PopplerExtractor(QObject* parent)
    : QObject(parent), m_runner(new ProcessRunner(this)) {}

PopplerExtractor::~PopplerExtractor() { cancel(); }

bool PopplerExtractor::isAvailable() const {
    if (m_poppler_path.isEmpty()) return false;
    QFileInfo info(m_poppler_path);
    return info.exists() && info.isExecutable();
}

QString PopplerExtractor::version() const {
    if (!isAvailable()) return {};
    QProcess proc;
    proc.start(m_poppler_path, {QStringLiteral("-v")});
    if (!proc.waitForStarted(3000) || !proc.waitForFinished(5000)) return {};
    // pdftotext 版本输出到 stderr
    QString output = QString::fromUtf8(proc.readAllStandardError());
    QRegularExpression re("version\\s+([0-9]+\\.[0-9]+(?:\\.[0-9]+)?)");
    auto m = re.match(output);
    return m.hasMatch() ? m.captured(1) : output.trimmed().left(50);
}

// ─── 提取文本（内存结果，含页码）────────────────────────

TextExtractionResult PopplerExtractor::extractText(const QString& pdf_path,
                                                   bool layout_mode) {
    TextExtractionResult result;

    if (!isAvailable()) {
        result.error      = QStringLiteral("Poppler 不可用");
        result.error_code = ConversionError::ToolMissing;
        return result;
    }
    QFileInfo info(pdf_path);
    if (!info.exists()) {
        result.error      = QStringLiteral("文件不存在: ") + pdf_path;
        result.error_code = ConversionError::SourceNotFound;
        return result;
    }

    // 先检查是否为扫描件
    if (isScannedPdf(pdf_path)) {
        result.error      = errorToUserMessage(ConversionError::ScannedPdfNoOcr);
        result.error_code = ConversionError::ScannedPdfNoOcr;
        return result;
    }

    // 构建命令
    QStringList args;
    if (layout_mode) args << "-layout";
    args << "-enc" << "UTF-8" << pdf_path << "-";

    emit logMessage(QStringLiteral("执行: ") + m_poppler_path + " " + args.join(' '));

    QElapsedTimer timer;
    timer.start();
    ProcResult r = m_runner->run(m_poppler_path, args, info.dir().path(), {}, 60000);

    if (r.timed_out) {
        result.error      = QStringLiteral("提取超时");
        result.error_code = ConversionError::Timeout;
        return result;
    }
    if (m_cancelled) {
        result.error      = QStringLiteral("提取已取消");
        result.error_code = ConversionError::Cancelled;
        m_cancelled = false;
        return result;
    }
    if (r.exit_code != 0) {
        QString err = r.std_err.toLower();
        if (err.contains("password") || err.contains("encrypted")) {
            result.error      = errorToUserMessage(ConversionError::PasswordProtected);
            result.error_code = ConversionError::PasswordProtected;
        } else if (err.contains("corrupt") || err.contains("invalid")) {
            result.error      = QStringLiteral("PDF 文件损坏");
            result.error_code = ConversionError::CorruptFile;
        } else {
            result.error      = QStringLiteral("提取失败: ") + r.std_err;
            result.error_code = ConversionError::Unknown;
        }
        return result;
    }

    QString raw_text = r.std_out;

    // 二次检查：提取出的文本太少也算扫描件
    int word_count = 0;
    bool in_word = false;
    for (const QChar& c : raw_text) {
        if (c.isLetterOrNumber()) { if (!in_word) { in_word = true; word_count++; } }
        else { in_word = false; }
    }
    if (word_count < 10) {
        result.error      = errorToUserMessage(ConversionError::ScannedPdfNoOcr);
        result.error_code = ConversionError::ScannedPdfNoOcr;
        return result;
    }

    result.plain_text    = raw_text;
    result.markdown_text = raw_text;
    parsePagesWithTracking(raw_text, result);
    result.ok = true;

    emit progress(100);
    return result;
}

// ─── 提取并写入文件 ────────────────────────────────────

TaskOutput PopplerExtractor::extractToFile(const QString& pdf_path,
                                           const QString& output_path,
                                           bool layout_mode) {
    TaskOutput output;
    output.status = TaskStatus::Running;

    QElapsedTimer timer;
    timer.start();

    TextExtractionResult extract = extractText(pdf_path, layout_mode);
    output.duration_ms = timer.elapsed();

    if (!extract.ok) {
        output.status        = TaskStatus::Failed;
        output.error_code    = extract.error_code;
        output.error_message = extract.error;
        return output;
    }

    QFileInfo out_info(output_path);
    QDir(out_info.dir()).mkpath(".");

    QFile f(output_path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        output.status        = TaskStatus::Failed;
        output.error_code    = ConversionError::Unknown;
        output.error_message = QStringLiteral("无法写入输出文件: ") + output_path;
        return output;
    }
    f.write(extract.markdown_text.toUtf8());
    f.close();

    output.status       = TaskStatus::Completed;
    output.product_path = output_path;
    output.logs << QStringLiteral("PDF 提取成功")
                << QStringLiteral("耗时: %1 ms").arg(output.duration_ms);
    return output;
}

void PopplerExtractor::cancel() {
    m_cancelled = true;
    if (m_runner) m_runner->cancel();
}

// ─── 逐页解析（保留页码）────────────────────────────────

void PopplerExtractor::parsePagesWithTracking(const QString& raw_text,
                                              TextExtractionResult& result) const {
    // pdftotext 默认用 form feed (\x0c) 分隔页面
    QStringList pages = raw_text.split('\x0c', Qt::SkipEmptyParts);

    MarkdownParser parser;
    QVector<StructBlock> all_blocks;
    QVector<SourceSpan>  all_spans;

    for (int page_num = 1; page_num <= pages.size(); ++page_num) {
        const QString& page_text = pages[page_num - 1];
        if (page_text.trimmed().isEmpty()) continue;

        QVector<StructBlock> page_blocks = parser.parse(page_text, page_num);
        QVector<SourceSpan>  page_spans  = parser.buildSpans(page_blocks);

        all_blocks += page_blocks;
        all_spans  += page_spans;
    }

    result.blocks = all_blocks;
    result.spans  = all_spans;
}

// ─── 扫描件检测 ─────────────────────────────────────────

bool PopplerExtractor::isScannedPdf(const QString& pdf_path) const {
    if (!isAvailable()) return false;

    // 提取少量文本做探测
    QStringList args{"-layout", "-enc", "UTF-8", "-l", "2", pdf_path, "-"};
    ProcResult r = m_runner->run(m_poppler_path, args, {}, {}, 15000);
    if (r.exit_code != 0) return false;

    // 统计可提取的单词数
    int word_count = 0;
    bool in_word = false;
    for (const QChar& c : r.std_out) {
        if (c.isLetterOrNumber()) { if (!in_word) { in_word = true; word_count++; } }
        else { in_word = false; }
    }

    // 同时检查文本中是否包含大量空白（扫描件的 pdftotext 输出通常大段空白）
    int total_chars = r.std_out.size();
    int non_space   = 0;
    for (const QChar& c : r.std_out) {
        if (!c.isSpace()) non_space++;
    }

    // 综合判断：
    // 1) 单词数 < 10 → 几乎肯定是扫描件
    // 2) 非空字符占比 < 5% 且总字符 > 200 → 大量空白，可能是扫描件
    if (word_count < 10) return true;
    if (total_chars > 200 && (double)non_space / total_chars < 0.05) return true;

    return false;
}

} // namespace conversion
} // namespace dmc
