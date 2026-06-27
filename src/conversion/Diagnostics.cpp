#include "Diagnostics.h"
#include <QDateTime>
#include <QSysInfo>

namespace dmc {
namespace conversion {

Diagnostics::Diagnostics(QObject* parent) : QObject(parent) {}
Diagnostics::~Diagnostics() {}

ConversionCapabilities Diagnostics::checkAllTools() const {
    if (!m_tool_runner) return {};
    auto caps = m_tool_runner->checkAllTools();
    emit const_cast<Diagnostics*>(this)->diagnosticsCompleted(caps);
    return caps;
}

ToolStatus Diagnostics::checkTool(const QString& tool_name) const {
    if (!m_tool_runner) {
        ToolStatus s;
        s.name = tool_name;
        s.available = false;
        s.error_message = QStringLiteral("工具查找器未设置");
        return s;
    }
    return m_tool_runner->checkTool(tool_name);
}

QVector<ToolStatus> Diagnostics::getAllToolStatus() const {
    return {checkTool("pandoc"), checkTool("tectonic"), checkTool("pdftotext")};
}

bool Diagnostics::canConvert(Format source, Format target) const {
    auto caps = checkAllTools();
    auto dir  = directionFromFormats(source, target);
    switch (dir) {
        case ConversionDirection::MD_to_DOCX:
        case ConversionDirection::DOCX_to_MD:
        case ConversionDirection::MD_to_HTML:
        case ConversionDirection::HTML_to_MD:
            return caps.pandoc_ok;
        case ConversionDirection::MD_to_PDF:
            return caps.pandoc_ok && caps.tectonic_ok;
        case ConversionDirection::PDF_to_MD:
            return caps.poppler_ok;
        default:
            return false;
    }
}

QVector<ConversionDirection> Diagnostics::supportedDirections() const {
    auto caps = checkAllTools();
    QVector<ConversionDirection> dirs;
    if (caps.pandoc_ok)
        dirs << ConversionDirection::MD_to_DOCX << ConversionDirection::DOCX_to_MD
             << ConversionDirection::MD_to_HTML << ConversionDirection::HTML_to_MD;
    if (caps.pandoc_ok && caps.tectonic_ok)
        dirs << ConversionDirection::MD_to_PDF;
    if (caps.poppler_ok)
        dirs << ConversionDirection::PDF_to_MD;
    return dirs;
}

QString Diagnostics::generateReport() const {
    QString r;
    r += QStringLiteral("========== 工具诊断报告 ==========\n");
    r += QStringLiteral("时间: %1\n").arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    r += QStringLiteral("系统: %1 (%2)\n\n")
             .arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture());

    for (const auto& s : getAllToolStatus()) {
        r += QStringLiteral("[%1]\n").arg(s.name);
        if (s.available) {
            r += QStringLiteral("  状态: ✓ 可用\n");
            r += QStringLiteral("  版本: %1\n").arg(s.version);
            r += QStringLiteral("  路径: %1\n").arg(s.path);
        } else {
            r += QStringLiteral("  状态: ✗ 不可用\n");
            r += QStringLiteral("  原因: %1\n")
                     .arg(s.error_message.value_or(QStringLiteral("未知")));
        }
        r += '\n';
    }

    r += QStringLiteral("--- 支持的转换 ---\n");
    auto dirs = supportedDirections();
    if (dirs.isEmpty()) {
        r += QStringLiteral("  (无可用工具)\n");
    }
    for (auto d : dirs) {
        QString src, tgt;
        switch (d) {
            case ConversionDirection::MD_to_DOCX: src = "Markdown"; tgt = "DOCX"; break;
            case ConversionDirection::DOCX_to_MD: src = "DOCX";     tgt = "Markdown"; break;
            case ConversionDirection::MD_to_HTML: src = "Markdown"; tgt = "HTML"; break;
            case ConversionDirection::HTML_to_MD: src = "HTML";     tgt = "Markdown"; break;
            case ConversionDirection::MD_to_PDF:  src = "Markdown"; tgt = "PDF"; break;
            case ConversionDirection::PDF_to_MD:  src = "PDF";      tgt = "Markdown"; break;
            default: continue;
        }
        r += QStringLiteral("  ✓ %1 → %2\n").arg(src, tgt);
    }
    r += QStringLiteral("==================================\n");
    return r;
}

} // namespace conversion
} // namespace dmc
