#include "ConversionService.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrent>

namespace dmc {
namespace conversion {

ConversionService::ConversionService(QObject* parent) : QObject(parent) {
    initComponents();
    connectSignals();
}

ConversionService::~ConversionService() {
    cancelAllTasks();
}

// ─── 初始化 ─────────────────────────────────────────────

void ConversionService::initComponents() {
    m_tool_runner = std::make_unique<ToolRunner>();
    m_pandoc      = std::make_unique<PandocConverter>();
    m_tectonic    = std::make_unique<TectonicConverter>();
    m_poppler     = std::make_unique<PopplerExtractor>();
    m_res_mgr     = std::make_unique<ResourceManager>();
    m_engine      = std::make_unique<ConversionEngine>();
    m_diagnostics = std::make_unique<Diagnostics>();

    // 原生转换器（始终创建，不依赖外部工具）
    m_native_md   = std::make_unique<NativeMarkdownConverter>();
    m_native_docx = std::make_unique<NativeDocxConverter>();
    m_native_pdf  = std::make_unique<NativePdfConverter>();

    // 工具路径
    m_pandoc->setPandocPath(m_tool_runner->pandocPath());
    m_tectonic->setTectonicPath(m_tool_runner->tectonicPath());
    m_poppler->setPopplerPath(m_tool_runner->popplerPath());

    // 资源目录
    QString data_dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_res_mgr->setResourceRoot(data_dir + "/conversion_resources");

    // 引擎 ← 工具组件
    m_engine->setPandocConverter(m_pandoc.get());
    m_engine->setTectonicConverter(m_tectonic.get());
    m_engine->setPopplerExtractor(m_poppler.get());
    m_engine->setResourceManager(m_res_mgr.get());

    // 引擎 ← 原生降级组件
    m_engine->setNativeMarkdownConverter(m_native_md.get());
    m_engine->setNativeDocxConverter(m_native_docx.get());
    m_engine->setNativePdfConverter(m_native_pdf.get());

    // 诊断 ← 工具查找器
    m_diagnostics->setToolRunner(m_tool_runner.get());
}

void ConversionService::connectSignals() {
    // 引擎信号 → 自身信号（透传）
    connect(m_engine.get(), &ConversionEngine::taskSubmitted,
            this, &ConversionService::taskSubmitted);
    connect(m_engine.get(), &ConversionEngine::taskStarted,
            this, &ConversionService::taskStarted);
    connect(m_engine.get(), &ConversionEngine::taskProgress,
            this, &ConversionService::taskProgress);

    connect(m_engine.get(), &ConversionEngine::taskCompleted,
            this, [this](TaskHandle h, const TaskOutput& o) {
                m_completed++;
                emit taskCompleted(h, o);
            });
    connect(m_engine.get(), &ConversionEngine::taskFailed,
            this, [this](TaskHandle h, const TaskOutput& o) {
                m_failed++;
                emit taskFailed(h, o);
            });
    connect(m_engine.get(), &ConversionEngine::taskCancelled,
            this, &ConversionService::taskCancelled);
    connect(m_engine.get(), &ConversionEngine::taskLog,
            this, &ConversionService::taskLog);
}

// ─── 文件→文件转换 ──────────────────────────────────────

TaskHandle ConversionService::submitConversion(const TaskInput& input) {
    m_total++;
    return m_engine->submitTask(input);
}

// ─── 异步文本提取 ───────────────────────────────────────

void ConversionService::extractTextAsync(const TextExtractionRequest& req) {
    // 用 QtConcurrent 在线程池执行，不阻塞 UI 线程
    auto future = QtConcurrent::run([this, req]() -> TextExtractionResult {
        return extractText(req);
    });

    auto* watcher = new QFutureWatcher<TextExtractionResult>(this);
    connect(watcher, &QFutureWatcher<TextExtractionResult>::finished, this,
            [this, req, watcher]() {
                TextExtractionResult result = watcher->result();
                watcher->deleteLater();
                if (result.ok)
                    emit extractionFinished(req, result);
                else
                    emit extractionFailed(req, result.error_code, result.error);
            });
    watcher->setFuture(future);
}

// ─── 同步文本提取 ───────────────────────────────────────

TextExtractionResult ConversionService::extractText(const TextExtractionRequest& req) {
    Format fmt = Format::Unknown;
    if (!req.source_format.isEmpty())
        fmt = formatFromString(req.source_format);

    // 尝试从文件路径推断格式
    if (fmt == Format::Unknown && !req.source_path.isEmpty()) {
        int dot = req.source_path.lastIndexOf('.');
        if (dot >= 0)
            fmt = formatFromString(req.source_path.mid(dot + 1));
    }

    // ── 内存源优先 ──
    if (!req.source_content.isEmpty()) {
        // Markdown 内存源：直接解析，不调外部工具
        if (fmt == Format::Markdown || fmt == Format::Unknown) {
            TextExtractionResult result;
            MarkdownParser parser;
            result.markdown_text = req.source_content;
            result.plain_text    = parser.toPlainText(req.source_content);
            if (req.prefer_structure) {
                result.blocks = parser.parse(req.source_content);
                result.spans  = parser.buildSpans(result.blocks);
            }
            result.ok = true;
            return result;
        }

        // DOCX / HTML 内存源：用 Pandoc 转换
        if (m_pandoc && m_pandoc->isAvailable()) {
            return m_pandoc->extractFromContent(req.source_content, req.source_format);
        }

        TextExtractionResult result;
        result.error      = QStringLiteral("Pandoc 不可用，无法处理内存源");
        result.error_code = ConversionError::ToolMissing;
        return result;
    }

    // ── 文件源 ──
    if (!QFileInfo(req.source_path).exists()) {
        TextExtractionResult result;
        result.error      = QStringLiteral("源文件不存在: ") + req.source_path;
        result.error_code = ConversionError::SourceNotFound;
        return result;
    }

    if (fmt == Format::Unknown) {
        TextExtractionResult result;
        result.error      = QStringLiteral("无法识别文件格式");
        result.error_code = ConversionError::UnsupportedFormat;
        return result;
    }

    switch (fmt) {
        case Format::Markdown: {
            QFile f(req.source_path);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                TextExtractionResult result;
                result.error      = QStringLiteral("无法打开文件");
                result.error_code = ConversionError::SourceNotFound;
                return result;
            }
            QString content = QString::fromUtf8(f.readAll());
            f.close();

            TextExtractionResult result;
            MarkdownParser parser;
            result.markdown_text = content;
            result.plain_text    = parser.toPlainText(content);
            if (req.prefer_structure) {
                result.blocks = parser.parse(content);
                result.spans  = parser.buildSpans(result.blocks);
            }
            result.ok = true;
            return result;
        }

        case Format::DOCX:
            if (m_pandoc && m_pandoc->isAvailable())
                return m_pandoc->extractFromDocx(req.source_path);
            if (m_native_docx) {
                TaskInput ti;
                ti.source_path   = req.source_path;
                ti.source_format = Format::DOCX;
                ti.target_format = Format::Markdown;
                ti.output_path   = req.source_path + ".extracted.md";
                ti.overwrite_existing = true;
                TaskOutput to = m_native_docx->importFromDocx(ti);
                TextExtractionResult r;
                if (to.status == TaskStatus::Completed) {
                    QFile f(to.product_path.value_or(ti.output_path));
                    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                        r.markdown_text = QString::fromUtf8(f.readAll());
                        r.plain_text = r.markdown_text;
                        MarkdownParser parser;
                        r.blocks = parser.parse(r.markdown_text);
                        r.spans  = parser.buildSpans(r.blocks);
                        r.ok = true;
                        f.close();
                    }
                    QFile::remove(ti.output_path);
                } else {
                    r.error      = to.error_message.value_or(QStringLiteral("DOCX 提取失败"));
                    r.error_code = to.error_code;
                }
                return r;
            }
            {
                TextExtractionResult r;
                r.error = QStringLiteral("Pandoc 不可用");
                r.error_code = ConversionError::ToolMissing;
                return r;
            }

        case Format::HTML:
            if (m_pandoc && m_pandoc->isAvailable())
                return m_pandoc->extractFromHtml(req.source_path);
            // 降级：原生 HTML 提取
            if (m_native_md) {
                TaskInput ti;
                ti.source_path   = req.source_path;
                ti.source_format = Format::HTML;
                ti.target_format = Format::Markdown;
                ti.output_path   = req.source_path + ".extracted.md";
                ti.overwrite_existing = true;
                TaskOutput to = m_native_md->importFromHtml(ti);
                TextExtractionResult r;
                if (to.status == TaskStatus::Completed) {
                    QFile f(to.product_path.value_or(ti.output_path));
                    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                        r.markdown_text = QString::fromUtf8(f.readAll());
                        r.plain_text = r.markdown_text;
                        MarkdownParser parser;
                        r.blocks = parser.parse(r.markdown_text);
                        r.spans  = parser.buildSpans(r.blocks);
                        r.ok = true;
                        f.close();
                    }
                    QFile::remove(ti.output_path);
                } else {
                    r.error      = to.error_message.value_or(QStringLiteral("HTML 提取失败"));
                    r.error_code = to.error_code;
                }
                return r;
            }
            {
                TextExtractionResult r;
                r.error = QStringLiteral("Pandoc 不可用");
                r.error_code = ConversionError::ToolMissing;
                return r;
            }

        case Format::PDF:
            if (m_poppler && m_poppler->isAvailable())
                return m_poppler->extractText(
                    req.source_path,
                    req.layout_mode == TextExtractionRequest::Layout);
            // 降级：原生 PDF 提取
            if (m_native_pdf) {
                TaskInput ti;
                ti.source_path   = req.source_path;
                ti.source_format = Format::PDF;
                ti.target_format = Format::Markdown;
                ti.output_path   = req.source_path + ".extracted.md";
                ti.overwrite_existing = true;
                TaskOutput to = m_native_pdf->importFromPdf(ti);
                TextExtractionResult r;
                if (to.status == TaskStatus::Completed) {
                    QFile f(to.product_path.value_or(ti.output_path));
                    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                        r.markdown_text = QString::fromUtf8(f.readAll());
                        r.plain_text = r.markdown_text;
                        MarkdownParser parser;
                        r.blocks = parser.parse(r.markdown_text);
                        r.spans  = parser.buildSpans(r.blocks);
                        r.ok = true;
                        f.close();
                    }
                    QFile::remove(ti.output_path);
                } else {
                    r.error      = to.error_message.value_or(QStringLiteral("PDF 提取失败"));
                    r.error_code = to.error_code;
                }
                return r;
            }
            {
                TextExtractionResult r;
                r.error = QStringLiteral("Poppler 不可用");
                r.error_code = ConversionError::ToolMissing;
                return r;
            }

        default: {
            TextExtractionResult result;
            result.error      = QStringLiteral("不支持的格式");
            result.error_code = ConversionError::UnsupportedFormat;
            return result;
        }
    }
}

// ─── 任务管理 ───────────────────────────────────────────

bool ConversionService::cancelTask(TaskHandle handle) {
    return m_engine->cancelTask(handle);
}

void ConversionService::cancelAllTasks() {
    m_engine->cancelAllTasks();
}

TaskOutput ConversionService::getTaskStatus(TaskHandle handle) const {
    return m_engine->getTaskStatus(handle);
}

// ─── 能力查询 ───────────────────────────────────────────

ConversionCapabilities ConversionService::capabilities() const {
    return m_tool_runner->checkAllTools();
}

bool ConversionService::canConvert(Format s, Format t) const {
    // 外部工具优先
    if (m_diagnostics->canConvert(s, t)) return true;
    // 原生降级：支持全部 6 个方向
    auto dir = directionFromFormats(s, t);
    return dir != ConversionDirection::Unknown;
}

QVector<ConversionDirection> ConversionService::supportedDirections() const {
    // 原生转换器支持全部方向
    return {
        ConversionDirection::MD_to_DOCX, ConversionDirection::DOCX_to_MD,
        ConversionDirection::MD_to_HTML, ConversionDirection::HTML_to_MD,
        ConversionDirection::MD_to_PDF,  ConversionDirection::PDF_to_MD,
    };
}

ServiceStats ConversionService::stats() const {
    ServiceStats s;
    s.total_tasks     = m_total;
    s.completed_tasks = m_completed;
    s.failed_tasks    = m_failed;
    s.pending_tasks   = m_engine->pendingTaskCount();
    s.running_tasks   = m_engine->runningTaskCount();
    return s;
}

// ─── 配置 ───────────────────────────────────────────────

void ConversionService::setPandocPath(const QString& path) {
    m_tool_runner->setPandocPath(path);
    m_pandoc->setPandocPath(path);
    emit capabilitiesChanged(capabilities());
}

void ConversionService::setTectonicPath(const QString& path) {
    m_tool_runner->setTectonicPath(path);
    m_tectonic->setTectonicPath(path);
    emit capabilitiesChanged(capabilities());
}

void ConversionService::setPopplerPath(const QString& path) {
    m_tool_runner->setPopplerPath(path);
    m_poppler->setPopplerPath(path);
    emit capabilitiesChanged(capabilities());
}

void ConversionService::setResourceRoot(const QString& path) {
    m_res_mgr->setResourceRoot(path);
}

void ConversionService::setMaxConcurrentTasks(int max) {
    m_engine->setMaxConcurrentTasks(max);
}

} // namespace conversion
} // namespace dmc
