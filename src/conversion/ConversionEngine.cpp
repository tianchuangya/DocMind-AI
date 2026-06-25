#include "ConversionEngine.h"
#include <QFileInfo>
#include <QDir>
#include <QTimer>
#include <QElapsedTimer>
#include <QtConcurrent/QtConcurrent>

namespace dmc {
namespace conversion {

ConversionEngine::ConversionEngine(QObject* parent) : QObject(parent) {}

ConversionEngine::~ConversionEngine() {
    cancelAllTasks();
}

TaskHandle ConversionEngine::submitTask(const TaskInput& input) {
    QMutexLocker lock(&m_mutex);

    TaskHandle h = m_next++;
    TaskInfo info;
    info.handle        = h;
    info.input         = input;
    info.output.status = TaskStatus::Pending;

    m_tasks[h] = info;
    m_queue.enqueue(h);

    emit taskSubmitted(h);
    QTimer::singleShot(0, this, &ConversionEngine::processNextTask);
    return h;
}

bool ConversionEngine::cancelTask(TaskHandle handle) {
    QMutexLocker lock(&m_mutex);

    // 在等待队列中
    if (m_queue.contains(handle)) {
        m_queue.removeOne(handle);
        if (m_tasks.contains(handle)) {
            m_tasks[handle].output.status      = TaskStatus::Cancelled;
            m_tasks[handle].output.error_code  = ConversionError::Cancelled;
            m_tasks[handle].output.error_message = QStringLiteral("任务已取消");
        }
        emit taskCancelled(handle);
        return true;
    }

    // 正在运行中 — 标记取消并通知子进程终止
    if (m_running.contains(handle)) {
        m_running[handle].output.status = TaskStatus::Cancelled;
        if (m_pandoc)   m_pandoc->cancel();
        if (m_tectonic) m_tectonic->cancel();
        if (m_poppler)  m_poppler->cancel();
        return true;
    }

    return false;
}

void ConversionEngine::cancelAllTasks() {
    QMutexLocker lock(&m_mutex);
    while (!m_queue.isEmpty()) {
        TaskHandle h = m_queue.dequeue();
        if (m_tasks.contains(h))
            m_tasks[h].output.status = TaskStatus::Cancelled;
        emit taskCancelled(h);
    }
    for (auto it = m_running.begin(); it != m_running.end(); ++it)
        it.value().output.status = TaskStatus::Cancelled;
    if (m_pandoc)   m_pandoc->cancel();
    if (m_tectonic) m_tectonic->cancel();
    if (m_poppler)  m_poppler->cancel();
}

TaskOutput ConversionEngine::getTaskStatus(TaskHandle handle) const {
    QMutexLocker lock(&m_mutex);
    if (m_tasks.contains(handle))   return m_tasks[handle].output;
    if (m_running.contains(handle)) return m_running[handle].output;
    TaskOutput o;
    o.status = TaskStatus::Failed;
    o.error_message = QStringLiteral("任务不存在");
    return o;
}

int ConversionEngine::pendingTaskCount() const {
    QMutexLocker lock(&m_mutex);
    return m_queue.size();
}

int ConversionEngine::runningTaskCount() const {
    QMutexLocker lock(&m_mutex);
    return m_running.size();
}

void ConversionEngine::processNextTask() {
    QMutexLocker lock(&m_mutex);

    if (m_queue.isEmpty() || m_running.size() >= m_max_concurrent)
        return;

    TaskHandle h = m_queue.dequeue();
    if (!m_tasks.contains(h)) return;

    TaskInfo info = m_tasks.take(h);
    info.output.status = TaskStatus::Running;
    m_running[h] = info;

    lock.unlock();
    emit taskStarted(h);

    // 用 QtConcurrent 在后台线程执行，避免阻塞事件循环
    auto input = info.input;
    auto future = QtConcurrent::run([this, input]() -> TaskOutput {
        return executeTask(input);
    });

    // 用 QFutureWatcher 在完成后回到主线程发信号
    auto* watcher = new QFutureWatcher<TaskOutput>(this);
    connect(watcher, &QFutureWatcher<TaskOutput>::finished, this, [this, h, watcher]() {
        TaskOutput output = watcher->result();
        watcher->deleteLater();

        QMutexLocker lock(&m_mutex);
        if (!m_running.contains(h)) return;

        m_running[h].output = output;

        if (output.status == TaskStatus::Completed)
            emit taskCompleted(h, output);
        else if (output.status == TaskStatus::Cancelled)
            emit taskCancelled(h);
        else
            emit taskFailed(h, output);

        // 归档
        m_tasks[h] = m_running.take(h);
        lock.unlock();

        // 处理下一个
        QTimer::singleShot(0, this, &ConversionEngine::processNextTask);
    });
    watcher->setFuture(future);
}

TaskOutput ConversionEngine::executeTask(const TaskInput& input) {
    auto dir = directionFromFormats(input.source_format, input.target_format);

    switch (dir) {
        // ── MD ↔ DOCX ──
        case ConversionDirection::MD_to_DOCX: {
            // 优先 Pandoc
            if (m_pandoc && m_pandoc->isAvailable())
                return m_pandoc->convert(input);
            // 降级：原生实现
            if (m_native_docx) {
                emit taskLog(0, QStringLiteral("Pandoc 不可用，使用原生 DOCX 转换"));
                return m_native_docx->exportToDocx(input);
            }
            break;
        }
        case ConversionDirection::DOCX_to_MD: {
            if (m_pandoc && m_pandoc->isAvailable())
                return m_pandoc->convert(input);
            if (m_native_docx) {
                emit taskLog(0, QStringLiteral("Pandoc 不可用，使用原生 DOCX 转换"));
                return m_native_docx->importFromDocx(input);
            }
            break;
        }

        // ── MD ↔ HTML ──
        case ConversionDirection::MD_to_HTML: {
            if (m_pandoc && m_pandoc->isAvailable())
                return m_pandoc->convert(input);
            if (m_native_md) {
                emit taskLog(0, QStringLiteral("Pandoc 不可用，使用原生 HTML 转换"));
                return m_native_md->exportToHtml(input);
            }
            break;
        }
        case ConversionDirection::HTML_to_MD: {
            if (m_pandoc && m_pandoc->isAvailable())
                return m_pandoc->convert(input);
            if (m_native_md) {
                emit taskLog(0, QStringLiteral("Pandoc 不可用，使用原生 HTML 转换"));
                return m_native_md->importFromHtml(input);
            }
            break;
        }

        // ── MD → PDF ──
        case ConversionDirection::MD_to_PDF: {
            // 优先 Tectonic + Pandoc
            if (m_tectonic && m_tectonic->isAvailable()) {
                QString pandoc_path = m_pandoc ? m_pandoc->pandocPath() : QString();
                return m_tectonic->convert(input, pandoc_path);
            }
            // 降级：原生 PDF
            if (m_native_pdf) {
                emit taskLog(0, QStringLiteral("Tectonic 不可用，使用原生 PDF 转换"));
                return m_native_pdf->exportToPdf(input);
            }
            break;
        }

        // ── PDF → MD ──
        case ConversionDirection::PDF_to_MD: {
            if (m_poppler && m_poppler->isAvailable())
                return m_poppler->extractToFile(input.source_path, input.output_path);
            if (m_native_pdf) {
                emit taskLog(0, QStringLiteral("Poppler 不可用，使用原生 PDF 提取"));
                return m_native_pdf->importFromPdf(input);
            }
            break;
        }

        default: break;
    }

    // 所有路径都失败
    TaskOutput o;
    o.status       = TaskStatus::Failed;
    o.error_code   = ConversionError::ToolMissing;
    o.error_message = QStringLiteral("无可用的转换器（外部工具和原生实现均不可用）");
    return o;
}

} // namespace conversion
} // namespace dmc
