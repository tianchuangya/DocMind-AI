#pragma once

#include "Types.h"
#include "ToolRunner.h"
#include <QObject>

namespace dmc {
namespace conversion {

/// 诊断服务 — 检查工具状态、生成报告
class Diagnostics : public QObject {
    Q_OBJECT
public:
    explicit Diagnostics(QObject* parent = nullptr);
    ~Diagnostics() override;

    void setToolRunner(ToolRunner* runner) { m_tool_runner = runner; }

    ConversionCapabilities checkAllTools() const;
    ToolStatus             checkTool(const QString& tool_name) const;
    QVector<ToolStatus>    getAllToolStatus() const;
    bool                   canConvert(Format source, Format target) const;
    QVector<ConversionDirection> supportedDirections() const;

    /// 生成可读的诊断报告
    QString generateReport() const;

signals:
    void diagnosticsCompleted(const ConversionCapabilities& caps);

private:
    ToolRunner* m_tool_runner{nullptr};
};

} // namespace conversion
} // namespace dmc
