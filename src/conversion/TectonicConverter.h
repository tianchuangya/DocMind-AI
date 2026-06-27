#pragma once

#include "Types.h"
#include "ProcessRunner.h"
#include <QObject>

namespace dmc {
namespace conversion {

/// Tectonic 转换器 — Markdown → LaTeX → PDF
class TectonicConverter : public QObject {
    Q_OBJECT
public:
    explicit TectonicConverter(QObject* parent = nullptr);
    ~TectonicConverter() override;

    void    setTectonicPath(const QString& p) { m_tectonic_path = p; }
    QString tectonicPath() const { return m_tectonic_path; }
    bool    isAvailable() const;
    QString version() const;

    /// Markdown → PDF（先 Pandoc 生成 .tex，再 Tectonic 编译 PDF）
    /// pandoc_path 由外部传入（本类不依赖 PandocConverter）
    TaskOutput convert(const TaskInput& input, const QString& pandoc_path = {});

    void cancel();

signals:
    void progress(int percent);
    void logMessage(const QString& message);

private:
    QString m_tectonic_path;
    ProcessRunner* m_runner{nullptr};
    bool m_cancelled{false};
};

} // namespace conversion
} // namespace dmc
