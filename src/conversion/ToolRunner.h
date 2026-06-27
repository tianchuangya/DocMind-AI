#pragma once

#include "Types.h"
#include <QFileInfo>
#include <QStringList>

namespace dmc {
namespace conversion {

/// 工具查找器 — 在 bundle 目录、PATH 和常见路径中查找 Pandoc / Tectonic / Poppler
class ToolRunner {
public:
    ToolRunner();

    QFileInfo findTool(const QString& tool_name) const;
    QString   getToolVersion(const QString& tool_path) const;
    ToolStatus checkTool(const QString& tool_name) const;
    ConversionCapabilities checkAllTools() const;

    QString pandocPath()    const { return m_pandoc_path; }
    QString tectonicPath()  const { return m_tectonic_path; }
    QString popplerPath()   const { return m_poppler_path; }

    void setPandocPath(const QString& p)   { m_pandoc_path = p; }
    void setTectonicPath(const QString& p) { m_tectonic_path = p; }
    void setPopplerPath(const QString& p)  { m_poppler_path = p; }

private:
    QString m_pandoc_path;
    QString m_tectonic_path;
    QString m_poppler_path;

    QStringList candidatePaths(const QString& tool_name) const;
};

} // namespace conversion
} // namespace dmc
