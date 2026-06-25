#pragma once

#include "Types.h"
#include <QObject>
#include <QProcess>
#include <QMap>
#include <memory>

namespace dmc {
namespace conversion {

/// 进程执行器 — 封装 QProcess，提供超时和取消
class ProcessRunner : public QObject {
    Q_OBJECT
public:
    explicit ProcessRunner(QObject* parent = nullptr);
    ~ProcessRunner() override;

    /// 同步执行（阻塞调用线程）
    ProcResult run(const QString& program,
                   const QStringList& args,
                   const QString& working_dir = {},
                   const QMap<QString, QString>& env = {},
                   int timeout_ms = 60000);

    /// 取消当前进程
    void cancel();

    /// 是否正在运行
    bool isRunning() const;

signals:
    void finished(int exit_code, const QString& std_out, const QString& std_err);
    void errorOccurred(QProcess::ProcessError error);

private:
    std::unique_ptr<QProcess> m_process;
    bool m_cancelled{false};
};

} // namespace conversion
} // namespace dmc
