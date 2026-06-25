#include "ProcessRunner.h"
#include <QEventLoop>
#include <QTimer>
#include <QElapsedTimer>
#include <QProcessEnvironment>

namespace dmc {
namespace conversion {

ProcessRunner::ProcessRunner(QObject* parent) : QObject(parent) {}

ProcessRunner::~ProcessRunner() {
    cancel();
}

ProcResult ProcessRunner::run(const QString& program,
                              const QStringList& args,
                              const QString& working_dir,
                              const QMap<QString, QString>& env,
                              int timeout_ms) {
    ProcResult result;
    QElapsedTimer timer;
    timer.start();

    m_process = std::make_unique<QProcess>();
    m_cancelled = false;

    if (!working_dir.isEmpty())
        m_process->setWorkingDirectory(working_dir);

    if (!env.isEmpty()) {
        QProcessEnvironment proc_env = QProcessEnvironment::systemEnvironment();
        for (auto it = env.cbegin(); it != env.cend(); ++it)
            proc_env.insert(it.key(), it.value());
        m_process->setProcessEnvironment(proc_env);
    }

    QEventLoop loop;
    connect(m_process.get(),
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            &loop, &QEventLoop::quit);
    connect(m_process.get(), &QProcess::errorOccurred,
            [&loop, this](QProcess::ProcessError) {
                m_cancelled = true;
                loop.quit();
            });

    // 超时计时器
    QTimer timeout_timer;
    bool timed_out = false;
    if (timeout_ms > 0) {
        connect(&timeout_timer, &QTimer::timeout, [&]() {
            timed_out = true;
            m_cancelled = true;
            if (m_process && m_process->state() != QProcess::NotRunning)
                m_process->kill();
            loop.quit();
        });
        timeout_timer.setSingleShot(true);
        timeout_timer.start(timeout_ms);
    }

    m_process->start(program, args);

    if (!m_process->waitForStarted(5000)) {
        result.exit_code = -1;
        result.std_err = QStringLiteral("无法启动进程: ") + program;
        result.duration_ms = timer.elapsed();
        return result;
    }

    loop.exec();

    if (timeout_timer.isActive())
        timeout_timer.stop();

    result.exit_code = m_process->exitCode();
    result.std_out = QString::fromUtf8(m_process->readAllStandardOutput());
    result.std_err = QString::fromUtf8(m_process->readAllStandardError());
    result.timed_out = timed_out;
    result.duration_ms = timer.elapsed();

    m_process.reset();
    m_cancelled = false;

    return result;
}

void ProcessRunner::cancel() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_cancelled = true;
        m_process->terminate();
        if (!m_process->waitForFinished(3000))
            m_process->kill();
    }
}

bool ProcessRunner::isRunning() const {
    return m_process && m_process->state() == QProcess::Running;
}

} // namespace conversion
} // namespace dmc
