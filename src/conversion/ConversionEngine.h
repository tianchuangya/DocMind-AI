#pragma once

#include "Types.h"
#include "PandocConverter.h"
#include "TectonicConverter.h"
#include "PopplerExtractor.h"
#include "ResourceManager.h"
#include "native/NativeMarkdownConverter.h"
#include "native/NativeDocxConverter.h"
#include "native/NativePdfConverter.h"
#include <QObject>
#include <QMutex>
#include <QQueue>
#include <QHash>

namespace dmc {
namespace conversion {

/// 转换引擎 — 异步任务队列，QObject 信号驱动
class ConversionEngine : public QObject {
    Q_OBJECT
public:
    explicit ConversionEngine(QObject* parent = nullptr);
    ~ConversionEngine() override;

    void setPandocConverter(PandocConverter* c)     { m_pandoc   = c; }
    void setTectonicConverter(TectonicConverter* c)  { m_tectonic = c; }
    void setPopplerExtractor(PopplerExtractor* e)    { m_poppler  = e; }
    void setResourceManager(ResourceManager* m)      { m_res_mgr  = m; }

    // 原生转换器（工具不可用时自动降级）
    void setNativeMarkdownConverter(NativeMarkdownConverter* c) { m_native_md  = c; }
    void setNativeDocxConverter(NativeDocxConverter* c)         { m_native_docx = c; }
    void setNativePdfConverter(NativePdfConverter* c)           { m_native_pdf  = c; }

    /// 提交转换任务（异步，立即返回 handle）
    TaskHandle submitTask(const TaskInput& input);

    /// 取消任务
    bool cancelTask(TaskHandle handle);

    /// 取消所有任务
    void cancelAllTasks();

    /// 查询任务状态
    TaskOutput getTaskStatus(TaskHandle handle) const;

    int pendingTaskCount() const;
    int runningTaskCount() const;

    void setMaxConcurrentTasks(int max) { m_max_concurrent = max; }

signals:
    void taskSubmitted(TaskHandle handle);
    void taskStarted(TaskHandle handle);
    void taskProgress(TaskHandle handle, int percent);
    void taskCompleted(TaskHandle handle, const TaskOutput& output);
    void taskFailed(TaskHandle handle, const TaskOutput& output);
    void taskCancelled(TaskHandle handle);
    void taskLog(TaskHandle handle, const QString& message);

private slots:
    void processNextTask();

private:
    struct TaskInfo {
        TaskHandle handle;
        TaskInput  input;
        TaskOutput output;
    };

    PandocConverter*   m_pandoc{nullptr};
    TectonicConverter* m_tectonic{nullptr};
    PopplerExtractor*  m_poppler{nullptr};
    ResourceManager*   m_res_mgr{nullptr};

    // 原生降级
    NativeMarkdownConverter* m_native_md{nullptr};
    NativeDocxConverter*     m_native_docx{nullptr};
    NativePdfConverter*      m_native_pdf{nullptr};

    QQueue<TaskHandle>         m_queue;
    QHash<TaskHandle, TaskInfo> m_tasks;
    QHash<TaskHandle, TaskInfo> m_running;
    TaskHandle m_next{1};
    int m_max_concurrent{1};
    mutable QMutex m_mutex;

    TaskOutput executeTask(const TaskInput& input);
};

} // namespace conversion
} // namespace dmc
