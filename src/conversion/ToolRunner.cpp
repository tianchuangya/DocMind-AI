#include "ToolRunner.h"
#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

namespace dmc {
namespace conversion {

ToolRunner::ToolRunner() {
    auto pandoc = findTool(QStringLiteral("pandoc"));
    if (pandoc.exists()) m_pandoc_path = pandoc.absoluteFilePath();

    auto tectonic = findTool(QStringLiteral("tectonic"));
    if (tectonic.exists()) m_tectonic_path = tectonic.absoluteFilePath();

    auto pdftotext = findTool(QStringLiteral("pdftotext"));
    if (pdftotext.exists()) m_poppler_path = pdftotext.absoluteFilePath();
}

QStringList ToolRunner::candidatePaths(const QString& tool_name) const {
    QStringList paths;

    // 1) 应用 bundle 内的工具
    QString app_dir = QCoreApplication::applicationDirPath();
    paths << app_dir + "/../Resources/tools/" + tool_name
          << app_dir + "/tools/" + tool_name
          << app_dir + "/bundled_tools/" + tool_name
          << app_dir + "/" + tool_name;

    // 2) 用户目录
    QString home = QDir::homePath();
    paths << home + "/.local/bin/" + tool_name
          << home + "/bin/" + tool_name;

    // 3) 系统 PATH
    QString sys = QStandardPaths::findExecutable(tool_name);
    if (!sys.isEmpty()) paths << sys;

    // 4) 常见安装路径
    paths << "/usr/local/bin/" + tool_name
          << "/opt/homebrew/bin/" + tool_name
          << "/usr/bin/" + tool_name;

    paths.removeDuplicates();
    paths.removeAll(QString());
    return paths;
}

QFileInfo ToolRunner::findTool(const QString& tool_name) const {
    for (const QString& path : candidatePaths(tool_name)) {
        QFileInfo info(path);
        if (info.exists() && info.isExecutable())
            return info;
    }
    return {};
}

QString ToolRunner::getToolVersion(const QString& tool_path) const {
    if (tool_path.isEmpty()) return {};

    QProcess proc;
    proc.start(tool_path, {QStringLiteral("--version")});
    if (!proc.waitForStarted(3000))  return {};
    if (!proc.waitForFinished(5000)) return {};

    QString output = QString::fromUtf8(proc.readAllStandardOutput());
    QString tool_name = QFileInfo(tool_path).baseName();

    // 专用格式
    QRegularExpression re(tool_name + "\\s+([0-9]+\\.[0-9]+(?:\\.[0-9]+)?)",
                          QRegularExpression::CaseInsensitiveOption);
    auto match = re.match(output);
    if (match.hasMatch()) return match.captured(1);

    // 通用格式
    QRegularExpression re2("([0-9]+\\.[0-9]+(?:\\.[0-9]+)?)");
    match = re2.match(output);
    if (match.hasMatch()) return match.captured(1);

    return output.trimmed().left(50);
}

ToolStatus ToolRunner::checkTool(const QString& tool_name) const {
    ToolStatus status;
    status.name = tool_name;

    QFileInfo info = findTool(tool_name);
    if (!info.exists()) {
        status.available = false;
        status.error_message = QStringLiteral("工具未找到");
        return status;
    }
    if (!info.isExecutable()) {
        status.available = false;
        status.error_message = QStringLiteral("工具不可执行");
        return status;
    }

    status.path      = info.absoluteFilePath();
    status.version   = getToolVersion(status.path);
    status.available = true;
    return status;
}

ConversionCapabilities ToolRunner::checkAllTools() const {
    ConversionCapabilities caps;

    auto p = checkTool(QStringLiteral("pandoc"));
    caps.pandoc_ok      = p.available;
    caps.pandoc_version = p.version;
    caps.pandoc_path    = p.path;

    auto t = checkTool(QStringLiteral("tectonic"));
    caps.tectonic_ok      = t.available;
    caps.tectonic_version = t.version;
    caps.tectonic_path    = t.path;

    auto pp = checkTool(QStringLiteral("pdftotext"));
    caps.poppler_ok      = pp.available;
    caps.poppler_version = pp.version;
    caps.poppler_path    = pp.path;

    return caps;
}

} // namespace conversion
} // namespace dmc
