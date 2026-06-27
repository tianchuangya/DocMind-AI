#include "TectonicConverter.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QDateTime>
#include <QElapsedTimer>

namespace dmc {
namespace conversion {

TectonicConverter::TectonicConverter(QObject* parent)
    : QObject(parent), m_runner(new ProcessRunner(this)) {}

TectonicConverter::~TectonicConverter() { cancel(); }

bool TectonicConverter::isAvailable() const {
    if (m_tectonic_path.isEmpty()) return false;
    QFileInfo info(m_tectonic_path);
    return info.exists() && info.isExecutable();
}

QString TectonicConverter::version() const {
    if (!isAvailable()) return {};
    QProcess proc;
    proc.start(m_tectonic_path, {QStringLiteral("--version")});
    if (!proc.waitForStarted(3000) || !proc.waitForFinished(5000)) return {};
    return QString::fromUtf8(proc.readAllStandardOutput()).trimmed().left(80);
}

TaskOutput TectonicConverter::convert(const TaskInput& input, const QString& pandoc_path) {
    TaskOutput output;
    output.status = TaskStatus::Running;

    if (!isAvailable()) {
        output.status      = TaskStatus::Failed;
        output.error_code  = ConversionError::ToolMissing;
        output.error_message = QStringLiteral("Tectonic 不可用");
        return output;
    }

    QFileInfo src_info(input.source_path);
    if (!input.is_memory_source && !src_info.exists()) {
        output.status      = TaskStatus::Failed;
        output.error_code  = ConversionError::SourceNotFound;
        output.error_message = QStringLiteral("源文件不存在: ") + input.source_path;
        return output;
    }

    if (input.output_path.isEmpty()) {
        output.status      = TaskStatus::Failed;
        output.error_code  = ConversionError::Unknown;
        output.error_message = QStringLiteral("输出路径为空");
        return output;
    }

    // 创建临时目录
    QString temp_dir = QDir::tempPath() + "/dmc_md2pdf_" +
                       QString::number(QDateTime::currentMSecsSinceEpoch());
    QDir().mkpath(temp_dir);

    QElapsedTimer timer;
    timer.start();

    // 步骤 1：Markdown → LaTeX（用 Pandoc）
    QString tex_path = temp_dir + "/output.tex";

    if (!pandoc_path.isEmpty() && QFileInfo(pandoc_path).exists()) {
        QStringList pandoc_args{
            "-f", "markdown", "-t", "latex",
            "--standalone", "--wrap=none",
            "-o", tex_path,
            input.source_path
        };
        if (!input.resource_path.isEmpty())
            pandoc_args << "--resource-path=" + input.resource_path;

        emit logMessage("Step 1: Pandoc MD → LaTeX");
        ProcResult pr = m_runner->run(pandoc_path, pandoc_args,
                                       src_info.dir().path(), {}, 60000);
        if (pr.exit_code != 0) {
            output.status      = TaskStatus::Failed;
            output.error_code  = ConversionError::Unknown;
            output.error_message = QStringLiteral("Pandoc 生成 LaTeX 失败: ") + pr.std_err;
            output.duration_ms = timer.elapsed();
            QDir(temp_dir).removeRecursively();
            return output;
        }
        emit progress(40);
    } else {
        // 没有 Pandoc，直接把 .md 当作 .tex 输入（降级处理）
        QFile::copy(input.source_path, tex_path);
    }

    if (m_cancelled) {
        output.status = TaskStatus::Cancelled;
        output.error_code = ConversionError::Cancelled;
        QDir(temp_dir).removeRecursively();
        return output;
    }

    // 步骤 2：LaTeX → PDF（用 Tectonic）
    QFileInfo out_info(input.output_path);
    QDir(out_info.dir()).mkpath(".");

    QStringList tectonic_args{tex_path, "--outdir", out_info.dir().path()};
    emit logMessage("Step 2: Tectonic LaTeX → PDF");

    ProcResult tr = m_runner->run(m_tectonic_path, tectonic_args,
                                   temp_dir, {}, 180000);

    output.duration_ms = timer.elapsed();
    output.exit_code   = tr.exit_code;

    // 清理临时文件
    QDir(temp_dir).removeRecursively();

    if (tr.timed_out) {
        output.status      = TaskStatus::Failed;
        output.error_code  = ConversionError::Timeout;
        output.error_message = QStringLiteral("PDF 编译超时");
        return output;
    }
    if (m_cancelled) {
        output.status      = TaskStatus::Cancelled;
        output.error_code  = ConversionError::Cancelled;
        m_cancelled = false;
        return output;
    }

    // Tectonic 输出的文件名默认是 input basename.pdf
    QString tectonic_output = out_info.dir().path() + "/output.pdf";
    if (QFileInfo(tectonic_output).exists() && tectonic_output != input.output_path) {
        QFile::rename(tectonic_output, input.output_path);
    }

    if (tr.exit_code != 0) {
        output.status = TaskStatus::Failed;
        QString err = (tr.std_out + tr.std_err).toLower();
        if (err.contains("undefined control sequence") || err.contains("missing")) {
            output.error_code    = ConversionError::CorruptFile;
            output.error_message = QStringLiteral("LaTeX 语法错误");
        } else if (err.contains("font") || err.contains("package")) {
            output.error_code    = ConversionError::ToolMissing;
            output.error_message = QStringLiteral("缺少字体或宏包");
        } else {
            output.error_code    = ConversionError::Unknown;
            output.error_message = QStringLiteral("PDF 编译失败: ") + tr.std_err;
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
    output.logs << QStringLiteral("PDF 转换成功")
                << QStringLiteral("耗时: %1 ms").arg(output.duration_ms);
    emit progress(100);
    return output;
}

void TectonicConverter::cancel() {
    m_cancelled = true;
    if (m_runner) m_runner->cancel();
}

} // namespace conversion
} // namespace dmc
