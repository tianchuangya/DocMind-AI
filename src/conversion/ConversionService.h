#pragma once

#include "Types.h"
#include "ConversionEngine.h"
#include "ToolRunner.h"
#include "PandocConverter.h"
#include "TectonicConverter.h"
#include "PopplerExtractor.h"
#include "MarkdownParser.h"
#include "ResourceManager.h"
#include "Diagnostics.h"
#include "native/NativeMarkdownConverter.h"
#include "native/NativePdfConverter.h"
#include <QObject>
#include <memory>

namespace dmc {
namespace conversion {

/// 转换服务 — 对外统一接口（模块 C / 模块 A 调用此类的实例）
///
/// 设计要点（对齐 PLAN.md + 模块 C 反馈）:
///   - 继承 QObject，信号驱动，不阻塞 UI 线程
///   - extractTextAsync 提供异步文本提取（知识库入库场景）
///   - TextExtractionRequest 支持内存源 (source_content)
///   - 批量提取逐文件回调（信号回传原始 req）
///   - 临时文件由本服务负责清理
class ConversionService : public QObject {
    Q_OBJECT
public:
    explicit ConversionService(QObject* parent = nullptr);
    ~ConversionService() override;

    // ── 文件→文件转换 ──
    TaskHandle submitConversion(const TaskInput& input);

    // ── 文本提取（异步，不阻塞调用线程）──
    /// 立即返回；完成后发射 extractionFinished / extractionFailed
    void extractTextAsync(const TextExtractionRequest& req);

    /// 同步版本（供测试和 CLI 使用）
    TextExtractionResult extractText(const TextExtractionRequest& req);

    // ── 任务管理 ──
    bool       cancelTask(TaskHandle handle);
    void       cancelAllTasks();
    TaskOutput getTaskStatus(TaskHandle handle) const;

    // ── 能力查询 ──
    ConversionCapabilities       capabilities() const;
    bool                         canConvert(Format s, Format t) const;
    QVector<ConversionDirection> supportedDirections() const;
    Diagnostics*                 diagnostics() const { return m_diagnostics.get(); }

    // ── 统计 ──
    ServiceStats stats() const;

    // ── 配置 ──
    void setPandocPath(const QString& path);
    void setTectonicPath(const QString& path);
    void setPopplerPath(const QString& path);
    void setResourceRoot(const QString& path);
    void setMaxConcurrentTasks(int max);

signals:
    // 转换任务生命周期
    void taskSubmitted(TaskHandle handle);
    void taskStarted(TaskHandle handle);
    void taskProgress(TaskHandle handle, int percent);
    void taskCompleted(TaskHandle handle, const TaskOutput& output);
    void taskFailed(TaskHandle handle, const TaskOutput& output);
    void taskCancelled(TaskHandle handle);
    void taskLog(TaskHandle handle, const QString& message);

    // 文本提取完成信号（模块 C 连接这两个）
    void extractionFinished(const TextExtractionRequest& req,
                            const TextExtractionResult& result);
    void extractionFailed(const TextExtractionRequest& req,
                          ConversionError code,
                          const QString& message);

    // 能力变化
    void capabilitiesChanged(const ConversionCapabilities& caps);

private:
    std::unique_ptr<ToolRunner>        m_tool_runner;
    std::unique_ptr<PandocConverter>   m_pandoc;
    std::unique_ptr<TectonicConverter> m_tectonic;
    std::unique_ptr<PopplerExtractor>  m_poppler;
    std::unique_ptr<ResourceManager>   m_res_mgr;
    std::unique_ptr<ConversionEngine>  m_engine;
    std::unique_ptr<Diagnostics>       m_diagnostics;

    // 原生转换器（不依赖外部工具，始终可用）
    std::unique_ptr<NativeMarkdownConverter> m_native_md;
    std::unique_ptr<NativePdfConverter>      m_native_pdf;

    size_t m_total{0}, m_completed{0}, m_failed{0};

    void initComponents();
    void connectSignals();
};

} // namespace conversion
} // namespace dmc
