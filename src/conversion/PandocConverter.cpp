#include "PandocConverter.h"
#include "MarkdownParser.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryFile>
#include <QElapsedTimer>

namespace dmc {
namespace conversion {

PandocConverter::PandocConverter(QObject* parent)
    : QObject(parent), m_runner(new ProcessRunner(this)) {}

PandocConverter::~PandocConverter() { cancel(); }

bool PandocConverter::isAvailable() const {
    if (m_pandoc_path.isEmpty()) return false;
    QFileInfo info(m_pandoc_path);
    return info.exists() && info.isExecutable();
}

QString PandocConverter::version() const {
    if (!isAvailable()) return {};
    QProcess proc;
    proc.start(m_pandoc_path, {QStringLiteral("--version")});
    if (!proc.waitForStarted(3000) || !proc.waitForFinished(5000)) return {};
    QString output = QString::fromUtf8(proc.readAllStandardOutput());
    QRegularExpression re("pandoc(?:\\.exe)?\\s+([0-9]+\\.[0-9]+(?:\\.[0-9]+)?)");
    auto m = re.match(output);
    return m.hasMatch() ? m.captured(1) : output.trimmed().left(50);
}

// ─── 文件→文件转换 ──────────────────────────────────────

TaskOutput PandocConverter::convert(const TaskInput& input) {
    TaskOutput output;
    output.status = TaskStatus::Running;

    if (!isAvailable()) {
        output.status      = TaskStatus::Failed;
        output.error_code  = ConversionError::ToolMissing;
        output.error_message = QStringLiteral("Pandoc 不可用");
        return output;
    }

    if (!input.is_memory_source) {
        QFileInfo src(input.source_path);
        if (!src.exists()) {
            output.status      = TaskStatus::Failed;
            output.error_code  = ConversionError::SourceNotFound;
            output.error_message = QStringLiteral("源文件不存在: ") + input.source_path;
            return output;
        }
    }

    if (input.output_path.isEmpty()) {
        output.status      = TaskStatus::Failed;
        output.error_code  = ConversionError::Unknown;
        output.error_message = QStringLiteral("输出路径为空");
        return output;
    }

    QFileInfo out_info(input.output_path);
    if (out_info.exists() && !input.overwrite_existing) {
        output.status      = TaskStatus::Failed;
        output.error_code  = ConversionError::Unknown;
        output.error_message = QStringLiteral("输出文件已存在: ") + input.output_path;
        return output;
    }
    QDir(out_info.dir()).mkpath(".");

    QStringList args = buildArgs(input);
    emit logMessage(QStringLiteral("执行: ") + m_pandoc_path + " " + args.join(' '));

    QElapsedTimer timer;
    timer.start();

    ProcResult r = m_runner->run(m_pandoc_path, args,
                                  QFileInfo(input.source_path).dir().path(),
                                  {}, 120000);

    output.duration_ms = timer.elapsed();
    output.exit_code   = r.exit_code;

    if (r.timed_out) {
        output.status      = TaskStatus::Failed;
        output.error_code  = ConversionError::Timeout;
        output.error_message = QStringLiteral("转换超时");
        return output;
    }
    if (m_cancelled) {
        output.status      = TaskStatus::Cancelled;
        output.error_code  = ConversionError::Cancelled;
        output.error_message = QStringLiteral("转换已取消");
        m_cancelled = false;
        return output;
    }
    if (r.exit_code != 0) {
        output.status = TaskStatus::Failed;
        QString err = (r.std_out + r.std_err).toLower();
        if (err.contains("password") || err.contains("encrypted")) {
            output.error_code = ConversionError::PasswordProtected;
            output.error_message = QStringLiteral("文件已加密，需要密码");
        } else if (err.contains("corrupt") || err.contains("invalid")) {
            output.error_code = ConversionError::CorruptFile;
            output.error_message = QStringLiteral("文件损坏或格式无效");
        } else if (err.contains("not found") || err.contains("no such file")) {
            output.error_code = ConversionError::SourceNotFound;
            output.error_message = QStringLiteral("源文件不存在");
        } else {
            output.error_code = ConversionError::Unknown;
            output.error_message = QStringLiteral("转换失败: ") + r.std_err;
        }
        return output;
    }

    if (!QFileInfo(input.output_path).exists()) {
        output.status      = TaskStatus::Failed;
        output.error_code  = ConversionError::Unknown;
        output.error_message = QStringLiteral("输出文件未生成");
        return output;
    }

    output.status       = TaskStatus::Completed;
    output.product_path = input.output_path;
    output.logs << QStringLiteral("转换成功")
                << QStringLiteral("耗时: %1 ms").arg(output.duration_ms);
    emit progress(100);
    return output;
}

// ─── DOCX 文本提取（内存结果）───────────────────────────

TextExtractionResult PandocConverter::extractFromDocx(const QString& docx_path) {
    TextExtractionResult result;
    if (!isAvailable()) {
        result.error = QStringLiteral("Pandoc 不可用");
        result.error_code = ConversionError::ToolMissing;
        return result;
    }
    QFileInfo info(docx_path);
    if (!info.exists()) {
        result.error = QStringLiteral("文件不存在: ") + docx_path;
        result.error_code = ConversionError::SourceNotFound;
        return result;
    }

    QStringList args{"-f", "docx", "-t", "markdown", "--wrap=none", docx_path};
    ProcResult r = m_runner->run(m_pandoc_path, args, info.dir().path(), {}, 60000);

    if (r.exit_code != 0) {
        QString err = r.std_err.toLower();
        if (err.contains("password") || err.contains("encrypted")) {
            result.error      = errorToUserMessage(ConversionError::PasswordProtected);
            result.error_code = ConversionError::PasswordProtected;
        } else {
            result.error      = QStringLiteral("DOCX 提取失败: ") + r.std_err;
            result.error_code = ConversionError::CorruptFile;
        }
        return result;
    }

    QString md = r.std_out;
    result.markdown_text = md;
    MarkdownParser parser;
    result.blocks = parser.parse(md);
    result.spans  = parser.buildSpans(result.blocks);
    result.plain_text = parser.toPlainText(md);
    result.ok = true;
    return result;
}

// ─── HTML 文本提取（内存结果）───────────────────────────

TextExtractionResult PandocConverter::extractFromHtml(const QString& html_path) {
    TextExtractionResult result;
    if (!isAvailable()) {
        result.error = QStringLiteral("Pandoc 不可用");
        result.error_code = ConversionError::ToolMissing;
        return result;
    }
    QFileInfo info(html_path);
    if (!info.exists()) {
        result.error = QStringLiteral("文件不存在: ") + html_path;
        result.error_code = ConversionError::SourceNotFound;
        return result;
    }

    QStringList args{"-f", "html", "-t", "markdown", "--wrap=none", html_path};
    ProcResult r = m_runner->run(m_pandoc_path, args, info.dir().path(), {}, 60000);

    if (r.exit_code != 0) {
        result.error      = QStringLiteral("HTML 提取失败: ") + r.std_err;
        result.error_code = ConversionError::CorruptFile;
        return result;
    }

    QString md = r.std_out;
    result.markdown_text = md;
    MarkdownParser parser;
    result.blocks = parser.parse(md);
    result.spans  = parser.buildSpans(result.blocks);
    result.plain_text = parser.toPlainText(md);
    result.ok = true;
    return result;
}

// ─── 从内存内容提取 ─────────────────────────────────────

TextExtractionResult PandocConverter::extractFromContent(const QString& content,
                                                         const QString& format_hint) {
    TextExtractionResult result;
    Format fmt = formatFromString(format_hint);

    // Markdown 内存源：直接解析，不需要外部工具
    if (fmt == Format::Markdown) {
        MarkdownParser parser;
        result.markdown_text = content;
        result.blocks        = parser.parse(content);
        result.spans         = parser.buildSpans(result.blocks);
        result.plain_text    = parser.toPlainText(content);
        result.ok = true;
        return result;
    }

    // DOCX / HTML 内存源：写入临时文件，用 Pandoc 转换后读取
    if (!isAvailable()) {
        result.error = QStringLiteral("Pandoc 不可用");
        result.error_code = ConversionError::ToolMissing;
        return result;
    }

    // 写临时文件
    QString ext = (fmt == Format::DOCX) ? ".docx" : ".html";
    QTemporaryFile tmp_file(QDir::tempPath() + "/dmc_extract_XXXXXX" + ext);
    tmp_file.setAutoRemove(true);
    if (!tmp_file.open()) {
        result.error      = QStringLiteral("无法创建临时文件");
        result.error_code = ConversionError::Unknown;
        return result;
    }
    tmp_file.write(content.toUtf8());
    tmp_file.flush();

    QStringList args{"-f", (fmt == Format::DOCX) ? "docx" : "html",
                     "-t", "markdown", "--wrap=none", tmp_file.fileName()};
    ProcResult r = m_runner->run(m_pandoc_path, args, {}, {}, 60000);

    tmp_file.close(); // 关闭后自动删除

    if (r.exit_code != 0) {
        result.error      = QStringLiteral("内存内容提取失败: ") + r.std_err;
        result.error_code = ConversionError::CorruptFile;
        return result;
    }

    QString md = r.std_out;
    result.markdown_text = md;
    MarkdownParser parser;
    result.blocks     = parser.parse(md);
    result.spans      = parser.buildSpans(result.blocks);
    result.plain_text = parser.toPlainText(md);
    result.ok = true;
    return result;
}

void PandocConverter::cancel() {
    m_cancelled = true;
    if (m_runner) m_runner->cancel();
}

QStringList PandocConverter::buildArgs(const TaskInput& input) const {
    QStringList args;

    switch (input.source_format) {
        case Format::Markdown: args << "-f" << "markdown"; break;
        case Format::DOCX:     args << "-f" << "docx";     break;
        case Format::HTML:     args << "-f" << "html";     break;
        default: break;
    }
    switch (input.target_format) {
        case Format::Markdown: args << "-t" << "markdown"; break;
        case Format::DOCX:     args << "-t" << "docx";     break;
        case Format::HTML:     args << "-t" << "html";     break;
        default: break;
    }

    args << "--wrap=none";

    if (input.source_format == Format::Markdown &&
        input.target_format == Format::HTML) {
        args << "--standalone" << "--self-contained";
    }
    if (input.source_format == Format::Markdown &&
        input.target_format == Format::DOCX) {
        args << "--standalone";
    }

    if (!input.resource_path.isEmpty())
        args << "--resource-path=" + input.resource_path;

    args << "-o" << input.output_path;

    if (input.is_memory_source)
        args << "-";  // stdin
    else
        args << input.source_path;

    return args;
}

} // namespace conversion
} // namespace dmc
