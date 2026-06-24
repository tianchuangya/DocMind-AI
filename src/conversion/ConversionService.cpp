#include "ConversionService.h"

#include <QProcess>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QMutex>
#include <QWaitCondition>

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace dmc {
namespace conversion {

QString errorToString(ConversionError e) {
    switch (e) {
        case ConversionError::None: return "None";
        case ConversionError::ToolMissing: return "ToolMissing";
        case ConversionError::SourceNotFound: return "SourceNotFound";
        case ConversionError::UnsupportedFormat: return "UnsupportedFormat";
        case ConversionError::CorruptFile: return "CorruptFile";
        case ConversionError::PasswordProtected: return "PasswordProtected";
        case ConversionError::ScannedPdfNoOcr: return "ScannedPdfNoOcr";
        case ConversionError::Timeout: return "Timeout";
        case ConversionError::Cancelled: return "Cancelled";
        default: return "Unknown";
    }
}

QString errorToUserMessage(ConversionError e) {
    switch (e) {
        case ConversionError::ToolMissing: return "Required conversion tool not installed.";
        case ConversionError::SourceNotFound: return "Source file not found.";
        case ConversionError::UnsupportedFormat: return "Format not supported.";
        case ConversionError::CorruptFile: return "File appears corrupted.";
        case ConversionError::PasswordProtected: return "PDF is encrypted. Provide password or skip.";
        case ConversionError::ScannedPdfNoOcr: return "Scanned PDF detected. OCR not available.";
        case ConversionError::Timeout: return "Conversion timed out.";
        case ConversionError::Cancelled: return "Operation cancelled.";
        default: return "Unknown error.";
    }
}

Format formatFromString(const QString& s) {
    QString l = s.toLower().trimmed();
    if (l == "markdown" || l == "md") return Format::Markdown;
    if (l == "docx") return Format::DOCX;
    if (l == "html" || l == "htm") return Format::HTML;
    if (l == "pdf") return Format::PDF;
    return Format::Unknown;
}

QString formatToString(Format f) {
    switch (f) {
        case Format::Markdown: return "Markdown";
        case Format::DOCX: return "DOCX";
        case Format::HTML: return "HTML";
        case Format::PDF: return "PDF";
        default: return "Unknown";
    }
}

ConversionDirection directionFromFormats(Format s, Format t) {
    if (s == Format::Markdown && t == Format::DOCX) return ConversionDirection::MD_to_DOCX;
    if (s == Format::DOCX && t == Format::Markdown) return ConversionDirection::DOCX_to_MD;
    if (s == Format::Markdown && t == Format::HTML) return ConversionDirection::MD_to_HTML;
    if (s == Format::HTML && t == Format::Markdown) return ConversionDirection::HTML_to_MD;
    if (s == Format::Markdown && t == Format::PDF) return ConversionDirection::MD_to_PDF;
    if (s == Format::PDF && t == Format::Markdown) return ConversionDirection::PDF_to_MD;
    return ConversionDirection::Unknown;
}

ProcResult runProcess(const QString& exe, const QStringList& args, int timeout_ms) {
    ProcResult res;
    QElapsedTimer timer;
    timer.start();

    QProcess proc;
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    proc.start(exe, args);

    if (!proc.waitForStarted(5000)) {
        res.exit_code = -1;
        res.std_err = "Failed to start process";
        return res;
    }

    bool finished = proc.waitForFinished(timeout_ms);
    if (!finished) {
        proc.kill();
        proc.waitForFinished(1000);
        res.timed_out = true;
    }

    res.exit_code = proc.exitCode();
    res.std_out = QString::fromUtf8(proc.readAllStandardOutput());
    res.std_err = QString::fromUtf8(proc.readAllStandardError());
    res.duration_ms = timer.elapsed();
    return res;
}

QString createTempDir() {
    QString base = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString path = base + "/dmc_conversion_XXXXXX";
    QDir dir;
    if (dir.mkpath(path)) return path;
    return "";
}

void cleanupDir(const QString& dir) {
    QDir(dir).removeRecursively();
}

bool fileExists(const QString& path) {
    return QFileInfo::exists(path);
}

QString readFileToString(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return "";
    QTextStream in(&f);
    return in.readAll();
}

bool isPasswordProtected(const QString& stderrText) {
    QString lower = stderrText.toLower();
    return lower.contains("password") || lower.contains("encrypted") || lower.contains("permission");
}

QVector<StructBlock> parseMarkdownBlocks(const QString& md, int page) {
    QVector<StructBlock> blocks;
    QStringList lines = md.split('\n');
    int lineNum = 0;
    bool inCodeBlock = false;
    StructBlock currentCode;
    QString currentPara;
    int paraStartLine = -1;

    auto flushPara = [&]() {
        if (!currentPara.isEmpty()) {
            StructBlock b;
            b.type = StructBlock::Paragraph;
            b.text = currentPara.trimmed();
            b.sourceLine = paraStartLine;
            b.sourcePage = page;
            blocks.append(b);
            currentPara.clear();
            paraStartLine = -1;
        }
    };

    for (const QString& line : lines) {
        lineNum++;
        QString trimmed = line.trimmed();

        if (trimmed.isEmpty() && !inCodeBlock) {
            flushPara();
            continue;
        }

        if (trimmed.startsWith("```")) {
            if (inCodeBlock) {
                currentCode.text.chop(1);
                blocks.append(currentCode);
                inCodeBlock = false;
                currentCode = StructBlock();
            } else {
                flushPara();
                inCodeBlock = true;
                currentCode.type = StructBlock::CodeBlock;
                currentCode.sourceLine = lineNum;
                currentCode.sourcePage = page;
                currentCode.text = "";
            }
            continue;
        }

        if (inCodeBlock) {
            currentCode.text += line + "\n";
            continue;
        }

        if (trimmed.startsWith('#')) {
            flushPara();
            int lvl = 0;
            while (lvl < trimmed.length() && trimmed[lvl] == '#') lvl++;
            StructBlock b;
            b.type = StructBlock::Heading;
            b.level = std::min(lvl, 6);
            b.text = trimmed.mid(lvl).trimmed();
            b.sourceLine = lineNum;
            b.sourcePage = page;
            blocks.append(b);
            continue;
        }

        if (trimmed.startsWith('>') || trimmed.startsWith("&gt;")) {
            flushPara();
            StructBlock b;
            b.type = StructBlock::Blockquote;
            QString txt = trimmed.startsWith('>') ? trimmed.mid(1) : trimmed;
            b.text = txt.trimmed();
            b.sourceLine = lineNum;
            b.sourcePage = page;
            blocks.append(b);
            continue;
        }

        QRegularExpression listRe("^[\\-\\*\\+]\\s|^[0-9]+\\.\\s");
        if (listRe.match(trimmed).hasMatch()) {
            flushPara();
            StructBlock b;
            b.type = StructBlock::ListItem;
            b.text = trimmed.replace(QRegularExpression("^[\\-\\*\\+0-9.]+\\s*"), "");
            b.sourceLine = lineNum;
            b.sourcePage = page;
            blocks.append(b);
            continue;
        }

        if (trimmed.contains('|') && trimmed.endsWith('|')) {
            flushPara();
            StructBlock b;
            b.type = StructBlock::TableCell;
            b.text = trimmed;
            b.sourceLine = lineNum;
            b.sourcePage = page;
            blocks.append(b);
            continue;
        }

        if (currentPara.isEmpty()) paraStartLine = lineNum;
        currentPara += (currentPara.isEmpty() ? "" : " ") + trimmed;
    }
    flushPara();
    return blocks;
}

QVector<SourceSpan> buildSourceSpans(const QVector<StructBlock>& blocks) {
    QVector<SourceSpan> spans;
    for (const auto& b : blocks) {
        SourceSpan sp;
        sp.page = b.sourcePage;
        sp.lineStart = b.sourceLine;
        if (b.type == StructBlock::Heading) {
            QString slug = b.text.toLower()
                .replace(QRegularExpression("[^a-z0-9\\s-]"), "")
                .replace(QRegularExpression("\\s+"), "-");
            sp.anchor = slug;
        }
        spans.append(sp);
    }
    return spans;
}

QString ToolRunner::bundledToolsDir() {
    return QCoreApplication::applicationDirPath() + "/bundled_tools";
}

std::optional<QString> ToolRunner::findExecutable(const QString& name, const QStringList& paths) {
    for (const auto& p : paths) {
        QString fp = p + "/" + name;
        if (QFileInfo::exists(fp) && QFileInfo(fp).isExecutable()) return fp;
    }
    QString bundled = bundledToolsDir() + "/" + name;
    if (QFileInfo::exists(bundled) && QFileInfo(bundled).isExecutable()) return bundled;

    QString found = QStandardPaths::findExecutable(name);
    if (!found.isEmpty()) return found;
    return std::nullopt;
}

bool PandocConverter::isAvailable() const {
    if (avail_.has_value()) return avail_.value();
    auto p = executablePath();
    avail_ = !p.isEmpty() && QFileInfo::exists(p);
    return avail_.value();
}

QString PandocConverter::executablePath() {
    auto r = ToolRunner::findExecutable("pandoc", {"./bundled_tools/pandoc"});
    return r.value_or("");
}

ProcResult PandocConverter::mdToDocx(const QString& in, const QString& out, const QString& res) {
    QStringList args = {in, "-o", out, "--standalone"};
    if (!res.isEmpty()) { args << "--resource-path" << res; }
    auto p = executablePath();
    return p.isEmpty() ? ProcResult{-1, "", "pandoc not found", false, 0} : runProcess(p, args);
}

ProcResult PandocConverter::docxToMd(const QString& in, const QString& out) {
    auto p = executablePath();
    return p.isEmpty() ? ProcResult{-1, "", "pandoc not found", false, 0} : runProcess(p, {in, "-o", out, "--to", "markdown"});
}

ProcResult PandocConverter::mdToHtml(const QString& in, const QString& out, const QString& res) {
    QStringList args = {in, "-o", out, "--standalone", "--self-contained"};
    if (!res.isEmpty()) { args << "--resource-path" << res; }
    auto p = executablePath();
    return p.isEmpty() ? ProcResult{-1, "", "pandoc not found", false, 0} : runProcess(p, args);
}

ProcResult PandocConverter::htmlToMd(const QString& in, const QString& out) {
    auto p = executablePath();
    return p.isEmpty() ? ProcResult{-1, "", "pandoc not found", false, 0} : runProcess(p, {in, "-o", out, "--from", "html", "--to", "markdown"});
}

bool TectonicConverter::isAvailable() const {
    if (avail_.has_value()) return avail_.value();
    auto p = executablePath();
    avail_ = !p.isEmpty() && QFileInfo::exists(p);
    return avail_.value();
}

QString TectonicConverter::executablePath() {
    auto r = ToolRunner::findExecutable("tectonic", {"./bundled_tools/tectonic"});
    return r.value_or("");
}

ProcResult TectonicConverter::mdToPdf(const QString& in, const QString& out, const QString& res) {
    QFileInfo fi(out);
    QString dir = fi.absolutePath();
    auto p = executablePath();
    return p.isEmpty() ? ProcResult{-1, "", "tectonic not found", false, 0} : runProcess(p, {in, "--outdir", dir});
}

bool PopplerExtractor::isAvailable() const {
    if (avail_.has_value()) return avail_.value();
    auto p = executablePath();
    avail_ = !p.isEmpty() && QFileInfo::exists(p);
    return avail_.value();
}

QString PopplerExtractor::executablePath() {
    auto r = ToolRunner::findExecutable("pdftotext", {"./bundled_tools/poppler"});
    return r.value_or("");
}

ProcResult PopplerExtractor::pdfToMd(const QString& in, const QString& out, TextExtractionRequest::LayoutMode mode) {
    auto p = executablePath();
    if (p.isEmpty()) return {-1, "", "pdftotext not found", false, 0};
    QStringList args;
    args << (mode == TextExtractionRequest::Raw ? "-raw" : "-layout");
    args << in << out;
    return runProcess(p, args);
}

ProcResult PopplerExtractor::pdfToStdout(const QString& in, TextExtractionRequest::LayoutMode mode) {
    auto p = executablePath();
    if (p.isEmpty()) return {-1, "", "pdftotext not found", false, 0};
    QStringList args;
    args << (mode == TextExtractionRequest::Raw ? "-raw" : "-layout");
    args << in << "-";
    return runProcess(p, args);
}

bool PopplerExtractor::isScannedPdf(const QString& path) const {
    auto p = executablePath();
    if (p.isEmpty()) return false;
    ProcResult r = runProcess(p, {"-raw", path, "-"});
    if (r.exit_code != 0) return false;

    QString text = r.std_out.trimmed();
    if (text.isEmpty()) return true;

    QStringList words = text.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    int wordCount = words.size();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return wordCount < 10;
    qint64 fileSize = file.size();
    file.close();

    double wordsPerPage = wordCount / std::max(1.0, fileSize / 30000.0);
    double textDensity = text.length() / std::max(1.0, (double)fileSize);

    return wordCount < 10 || (wordsPerPage < 20 && textDensity < 0.01);
}

QVector<ToolStatus> Diagnostics::checkAllTools() const {
    return {checkTool("pandoc"), checkTool("tectonic"), checkTool("pdftotext")};
}

ToolStatus Diagnostics::checkTool(const QString& name) const {
    ToolStatus s;
    s.name = name;
    auto p = ToolRunner::findExecutable(name, {"./bundled_tools"});
    if (p) {
        s.available = true;
        s.path = *p;
        ProcResult r = runProcess(*p, {"--version"});
        if (r.exit_code == 0 && !r.std_out.isEmpty()) {
            s.version = r.std_out.split('\n').first();
        } else {
            s.version = "unknown";
        }
    } else {
        s.available = false;
        s.error_message = "Not found";
    }
    return s;
}

ConversionCapabilities Diagnostics::getCapabilities() const {
    ConversionCapabilities cap;
    PandocConverter pandoc;
    TectonicConverter tectonic;
    PopplerExtractor poppler;

    cap.pandoc_ok = pandoc.isAvailable();
    cap.pandoc_path = PandocConverter::executablePath();
    cap.tectonic_ok = tectonic.isAvailable();
    cap.tectonic_path = TectonicConverter::executablePath();
    cap.poppler_ok = poppler.isAvailable();
    cap.poppler_path = PopplerExtractor::executablePath();
    return cap;
}

QString ResourceManager::createTempDir() {
    return dmc::conversion::createTempDir();
}

void ResourceManager::cleanupTempDir(const QString& dir) {
    dmc::conversion::cleanupDir(dir);
}

bool ResourceManager::confirmOverwrite(const QString& path) const {
    return QFileInfo::exists(path);
}

bool ResourceManager::copyFile(const QString& from, const QString& to) const {
    return QFile::copy(from, to);
}

QString ResourceManager::resolveRelativePath(const QString& base, const QString& rel) const {
    if (rel.isEmpty()) return base;
    if (rel.startsWith('/')) return rel;
    QFileInfo fi(base);
    return fi.absolutePath() + "/" + rel;
}

class ConversionEngine::TaskQueue {
    struct Entry {
        TaskHandle handle;
        TaskInput input;
        TaskOutput output;
        std::function<void(TaskHandle, const TaskOutput&)> callback;
        bool cancelled{false};
    };
    std::queue<std::shared_ptr<Entry>> pending_;
    std::map<TaskHandle, std::shared_ptr<Entry>> running_, completed_;
    mutable QMutex mu_;
    QWaitCondition cv_;
    std::atomic<bool> stop_{false};
    std::atomic<TaskHandle> nextHandle_{1};
    std::vector<std::thread> workers_;
    ConversionEngine* engine_;

public:
    TaskQueue(ConversionEngine* engine) : engine_(engine) {}
    ~TaskQueue() { stop(); }

    TaskHandle enqueue(const TaskInput& in, std::function<void(TaskHandle, const TaskOutput&)> cb = nullptr) {
        auto e = std::make_shared<Entry>();
        e->handle = nextHandle_++;
        e->input = in;
        e->callback = cb;
        e->output.status = TaskStatus::Pending;
        {
            QMutexLocker l(&mu_);
            pending_.push(e);
        }
        cv_.wakeOne();
        return e->handle;
    }

    bool cancel(TaskHandle h) {
        QMutexLocker l(&mu_);
        std::queue<std::shared_ptr<Entry>> newQ;
        while (!pending_.empty()) {
            auto e = pending_.front();
            pending_.pop();
            if (e->handle == h) {
                e->cancelled = true;
                e->output.status = TaskStatus::Cancelled;
                e->output.error_code = ConversionError::Cancelled;
                completed_[h] = e;
                if (e->callback) e->callback(e->handle, e->output);
                return true;
            }
            newQ.push(e);
        }
        pending_ = newQ;
        auto it = running_.find(h);
        if (it != running_.end()) {
            it->second->cancelled = true;
            return true;
        }
        return false;
    }

    TaskOutput getStatus(TaskHandle h) {
        QMutexLocker l(&mu_);
        auto it = completed_.find(h);
        if (it != completed_.end()) return it->second->output;
        it = running_.find(h);
        if (it != running_.end()) return it->second->output;
        return {};
    }

    void start(int n = 2) {
        for (int i = 0; i < n; ++i) {
            workers_.emplace_back([this] {
                while (!stop_.load()) {
                    std::shared_ptr<Entry> e;
                    {
                        QMutexLocker l(&mu_);
                        if (pending_.empty()) {
                            cv_.wait(&mu_);
                            if (stop_.load() && pending_.empty()) break;
                        }
                        if (!pending_.empty()) {
                            e = pending_.front();
                            pending_.pop();
                            running_[e->handle] = e;
                        }
                    }
                    if (e) {
                        engine_->processTask(e->handle);
                    }
                }
            });
        }
    }

    void stop() {
        stop_.store(true);
        cv_.wakeAll();
        for (auto& w : workers_) {
            if (w.joinable()) w.join();
        }
        workers_.clear();
    }

    void waitAll() {
        QMutexLocker l(&mu_);
        while (!pending_.empty() || !running_.empty()) {
            cv_.wait(&mu_);
        }
    }

    bool empty() const {
        QMutexLocker l(&mu_);
        return pending_.empty() && running_.empty();
    }

    void clear() {
        QMutexLocker l(&mu_);
        while (!pending_.empty()) pending_.pop();
    }

    size_t pendingCount() const {
        QMutexLocker l(&mu_);
        return pending_.size() + running_.size();
    }

    size_t runningCount() const {
        QMutexLocker l(&mu_);
        return running_.size();
    }

    std::shared_ptr<Entry> getEntry(TaskHandle h) {
        QMutexLocker l(&mu_);
        auto it = running_.find(h);
        if (it != running_.end()) return it->second;
        it = completed_.find(h);
        if (it != completed_.end()) return it->second;
        return nullptr;
    }

    void finishTask(TaskHandle h, const TaskOutput& out) {
        std::shared_ptr<Entry> e;
        {
            QMutexLocker l(&mu_);
            auto it = running_.find(h);
            if (it != running_.end()) {
                e = it->second;
                running_.erase(it);
            }
            if (e) {
                e->output = out;
                completed_[h] = e;
            }
        }
        if (e && e->callback) e->callback(h, out);
        cv_.wakeAll();
    }
};

ConversionEngine::ConversionEngine(QObject* parent)
    : QObject(parent), queue_(std::make_unique<TaskQueue>(this)) {
    qRegisterMetaType<TextExtractionRequest>();
    qRegisterMetaType<TextExtractionResult>();
    qRegisterMetaType<ConversionError>();
    qRegisterMetaType<TaskOutput>();
    queue_->start(2);
}

ConversionEngine::~ConversionEngine() {
    queue_->stop();
}

TaskHandle ConversionEngine::convert(const TaskInput& in) {
    std::unique_lock<std::shared_mutex> l(mu_);
    total_++;
    return queue_->enqueue(in, [this](TaskHandle h, const TaskOutput& o) {
        if (o.status == TaskStatus::Completed) done_++;
        else if (o.status == TaskStatus::Failed) fail_++;
        emit taskFinished(h, o);
    });
}

QVector<TaskHandle> ConversionEngine::convertBatch(const QVector<TaskInput>& inputs) {
    std::unique_lock<std::shared_mutex> l(mu_);
    QVector<TaskHandle> handles;
    handles.reserve(inputs.size());
    for (const auto& in : inputs) {
        total_++;
        handles.append(queue_->enqueue(in, [this](TaskHandle h, const TaskOutput& o) {
            if (o.status == TaskStatus::Completed) done_++;
            else if (o.status == TaskStatus::Failed) fail_++;
            emit taskFinished(h, o);
        }));
    }
    return handles;
}

TaskOutput ConversionEngine::getStatus(TaskHandle h) {
    std::shared_lock<std::shared_mutex> l(mu_);
    return queue_->getStatus(h);
}

bool ConversionEngine::cancel(TaskHandle h) {
    std::shared_lock<std::shared_mutex> l(mu_);
    return queue_->cancel(h);
}

void ConversionEngine::cancelAll() {
    std::shared_lock<std::shared_mutex> l(mu_);
    queue_->clear();
}

void ConversionEngine::wait(TaskHandle h) {
    while (true) {
        auto s = getStatus(h);
        if (s.status == TaskStatus::Completed || s.status == TaskStatus::Failed || s.status == TaskStatus::Cancelled) break;
        QThread::msleep(50);
    }
}

void ConversionEngine::waitAll() {
    std::shared_lock<std::shared_mutex> l(mu_);
    queue_->waitAll();
}

ServiceStats ConversionEngine::getStats() const {
    ServiceStats s;
    s.total_tasks = total_.load();
    s.completed_tasks = done_.load();
    s.failed_tasks = fail_.load();
    s.pending_tasks = queue_->pendingCount();
    s.running_tasks = queue_->runningCount();
    return s;
}

bool ConversionEngine::isToolAvailable(const QString& name) const {
    if (name == "pandoc") return PandocConverter().isAvailable();
    if (name == "tectonic") return TectonicConverter().isAvailable();
    if (name == "pdftotext") return PopplerExtractor().isAvailable();
    return false;
}

QVector<ConversionDirection> ConversionEngine::getSupportedDirections() const {
    return {
        ConversionDirection::MD_to_DOCX, ConversionDirection::DOCX_to_MD,
        ConversionDirection::MD_to_HTML, ConversionDirection::HTML_to_MD,
        ConversionDirection::MD_to_PDF, ConversionDirection::PDF_to_MD
    };
}

bool ConversionEngine::canConvert(Format s, Format t) const {
    return directionFromFormats(s, t) != ConversionDirection::Unknown;
}

ConversionCapabilities ConversionEngine::capabilities() const {
    return diag_.getCapabilities();
}

TextExtractionResult ConversionEngine::extractText(const TextExtractionRequest& req) {
    TextExtractionResult result;

    bool isMemorySource = !req.source_content.isEmpty();

    if (!isMemorySource && !fileExists(req.source_path)) {
        result.error = "File not found: " + req.source_path;
        result.error_code = ConversionError::SourceNotFound;
        return result;
    }

    Format srcFmt = Format::Unknown;
    if (!req.source_format.isEmpty()) {
        srcFmt = formatFromString(req.source_format);
    }
    if (srcFmt == Format::Unknown && !req.source_path.isEmpty()) {
        QFileInfo fi(req.source_path);
        srcFmt = formatFromString(fi.suffix());
    }
    if (srcFmt == Format::Unknown) {
        result.error = "Cannot determine format";
        result.error_code = ConversionError::UnsupportedFormat;
        return result;
    }

    QString content;
    if (isMemorySource) {
        content = req.source_content;
    } else {
        content = readFileToString(req.source_path);
    }

    if (srcFmt == Format::Markdown) {
        result.markdown_text = content;
        result.plain_text = content;
        if (req.prefer_structure) {
            result.blocks = parseMarkdownBlocks(content);
            result.spans = buildSourceSpans(result.blocks);
        }
        result.ok = true;
        return result;
    }

    auto caps = capabilities();
    if (!caps.canExtract(srcFmt)) {
        result.error = "Tool not available for " + formatToString(srcFmt);
        result.error_code = ConversionError::ToolMissing;
        return result;
    }

    QString tmpDir = createTempDir();
    if (tmpDir.isEmpty()) {
        result.error = "Failed to create temp dir";
        result.error_code = ConversionError::Unknown;
        return result;
    }
    QString tmpOut = tmpDir + "/extracted.md";
    bool needCleanup = true;
    auto cleanup = [&]() { if (needCleanup) cleanupDir(tmpDir); };

    if (srcFmt == Format::DOCX) {
        PandocConverter p;
        if (!p.isAvailable()) { cleanup(); result.error = "Pandoc missing"; result.error_code = ConversionError::ToolMissing; return result; }
        auto r = p.docxToMd(isMemorySource ? req.source_path : req.source_path, tmpOut);
        if (r.exit_code != 0) {
            cleanup();
            if (isPasswordProtected(r.std_err)) {
                result.error = errorToUserMessage(ConversionError::PasswordProtected);
                result.error_code = ConversionError::PasswordProtected;
            } else {
                result.error = "DOCX failed: " + r.std_err.left(200);
                result.error_code = ConversionError::CorruptFile;
            }
            return result;
        }
    } else if (srcFmt == Format::HTML) {
        PandocConverter p;
        if (!p.isAvailable()) { cleanup(); result.error = "Pandoc missing"; result.error_code = ConversionError::ToolMissing; return result; }
        auto r = p.htmlToMd(req.source_path, tmpOut);
        if (r.exit_code != 0) { cleanup(); result.error = "HTML failed"; result.error_code = ConversionError::CorruptFile; return result; }
    } else if (srcFmt == Format::PDF) {
        PopplerExtractor p;
        if (!p.isAvailable()) { cleanup(); result.error = "Poppler missing"; result.error_code = ConversionError::ToolMissing; return result; }

        auto testR = p.pdfToStdout(req.source_path, req.layout_mode);
        if (testR.exit_code != 0) {
            cleanup();
            if (isPasswordProtected(testR.std_err)) {
                result.error = errorToUserMessage(ConversionError::PasswordProtected);
                result.error_code = ConversionError::PasswordProtected;
            } else {
                result.error = "PDF failed: " + testR.std_err.left(200);
                result.error_code = ConversionError::CorruptFile;
            }
            return result;
        }

        if (p.isScannedPdf(req.source_path)) {
            cleanup();
            result.error = errorToUserMessage(ConversionError::ScannedPdfNoOcr);
            result.error_code = ConversionError::ScannedPdfNoOcr;
            return result;
        }

        QString raw = testR.std_out;
        QStringList pages = raw.split("\x0c");
        QVector<StructBlock> allBlocks;
        QVector<SourceSpan> allSpans;

        for (int i = 0; i < pages.size(); ++i) {
            if (pages[i].trimmed().isEmpty()) continue;
            auto pageBlocks = parseMarkdownBlocks(pages[i], i + 1);
            auto pageSpans = buildSourceSpans(pageBlocks);
            for (auto& b : pageBlocks) b.sourcePage = i + 1;
            for (auto& s : pageSpans) s.page = i + 1;
            allBlocks.append(pageBlocks);
            allSpans.append(pageSpans);
        }

        result.markdown_text = raw;
        result.plain_text = raw;
        if (req.prefer_structure) {
            result.blocks = allBlocks;
            result.spans = allSpans;
        }
        result.ok = true;
        needCleanup = false;
        cleanup();
        return result;
    }

    QString md = readFileToString(tmpOut);
    result.markdown_text = md;
    result.plain_text = md;
    if (req.prefer_structure) {
        result.blocks = parseMarkdownBlocks(md);
        result.spans = buildSourceSpans(result.blocks);
    }
    result.ok = true;
    needCleanup = false;
    cleanup();
    return result;
}

void ConversionEngine::extractTextAsync(const TextExtractionRequest& req) {
    QtConcurrent::run([this, req]() {
        TextExtractionResult result = this->extractText(req);
        if (result.ok) {
            emit extractionFinished(req, result);
        } else {
            emit extractionFailed(req, result.error_code, result.error);
        }
    });
}

void ConversionEngine::extractTextBatch(const QVector<TextExtractionRequest>& reqs) {
    batchCancelled_.store(false);
    batchTotal_ = reqs.size();
    batchCompleted_.store(0);

    QtConcurrent::run([this, reqs]() {
        for (const auto& req : reqs) {
            if (batchCancelled_.load()) break;

            TextExtractionResult result = this->extractText(req);
            if (result.ok) {
                emit extractionFinished(req, result);
            } else {
                emit extractionFailed(req, result.error_code, result.error);
            }

            int completed = batchCompleted_.fetch_add(1) + 1;
            emit batchProgress(completed, batchTotal_, req.source_path);
        }
        emit batchFinished();
    });
}

void ConversionEngine::cancelBatch() {
    batchCancelled_.store(true);
}

void ConversionEngine::processTask(TaskHandle handle) {
    auto entry = queue_->getEntry(handle);
    if (!entry) return;

    QElapsedTimer timer;
    timer.start();

    entry->output.status = TaskStatus::Running;
    entry->output.logs.append("Start: " + entry->input.source_path + " -> " + formatToString(entry->input.target_format));

    ProcResult r;
    bool ok = false;
    auto sf = entry->input.source_format;
    auto tf = entry->input.target_format;
    auto sp = entry->input.source_path;
    auto op = entry->input.output_path;
    auto rp = entry->input.resource_path;

    PandocConverter pandoc;
    TectonicConverter tectonic;
    PopplerExtractor poppler;

    auto dir = directionFromFormats(sf, tf);

    try {
        switch (dir) {
            case ConversionDirection::MD_to_DOCX:
                if (!pandoc.isAvailable()) { entry->output.error_code = ConversionError::ToolMissing; entry->output.error_message = "Pandoc missing"; break; }
                r = pandoc.mdToDocx(sp, op, rp); ok = (r.exit_code == 0); break;
            case ConversionDirection::DOCX_to_MD:
                if (!pandoc.isAvailable()) { entry->output.error_code = ConversionError::ToolMissing; entry->output.error_message = "Pandoc missing"; break; }
                r = pandoc.docxToMd(sp, op); ok = (r.exit_code == 0); break;
            case ConversionDirection::MD_to_HTML:
                if (!pandoc.isAvailable()) { entry->output.error_code = ConversionError::ToolMissing; entry->output.error_message = "Pandoc missing"; break; }
                r = pandoc.mdToHtml(sp, op, rp); ok = (r.exit_code == 0); break;
            case ConversionDirection::HTML_to_MD:
                if (!pandoc.isAvailable()) { entry->output.error_code = ConversionError::ToolMissing; entry->output.error_message = "Pandoc missing"; break; }
                r = pandoc.htmlToMd(sp, op); ok = (r.exit_code == 0); break;
            case ConversionDirection::MD_to_PDF:
                if (!tectonic.isAvailable()) { entry->output.error_code = ConversionError::ToolMissing; entry->output.error_message = "Tectonic missing"; break; }
                r = tectonic.mdToPdf(sp, op, rp); ok = (r.exit_code == 0); break;
            case ConversionDirection::PDF_to_MD:
                if (!poppler.isAvailable()) { entry->output.error_code = ConversionError::ToolMissing; entry->output.error_message = "Poppler missing"; break; }
                r = poppler.pdfToStdout(sp);
                if (r.exit_code != 0) {
                    if (isPasswordProtected(r.std_err)) { entry->output.error_code = ConversionError::PasswordProtected; entry->output.error_message = errorToUserMessage(ConversionError::PasswordProtected); }
                    else { entry->output.error_code = ConversionError::CorruptFile; entry->output.error_message = "PDF failed"; }
                    break;
                }
                if (poppler.isScannedPdf(sp)) { entry->output.error_code = ConversionError::ScannedPdfNoOcr; entry->output.error_message = errorToUserMessage(ConversionError::ScannedPdfNoOcr); }
                r = poppler.pdfToMd(sp, op); ok = (r.exit_code == 0); break;
            default:
                entry->output.error_code = ConversionError::UnsupportedFormat; entry->output.error_message = "Unsupported"; break;
        }

        if (entry->output.error_code != ConversionError::None && entry->output.error_code != ConversionError::ScannedPdfNoOcr) {
        } else {
            entry->output.exit_code = r.exit_code;
            if (r.timed_out) { entry->output.error_code = ConversionError::Timeout; entry->output.error_message = errorToUserMessage(ConversionError::Timeout); ok = false; }
        }
    } catch (const std::exception& ex) {
        entry->output.error_code = ConversionError::CorruptFile;
        entry->output.error_message = QString("Exception: ") + ex.what();
    }

    entry->output.duration_ms = timer.elapsed();

    TaskOutput out = entry->output;
    if (entry->cancelled) {
        out.status = TaskStatus::Cancelled;
        out.error_code = ConversionError::Cancelled;
    } else if (ok) {
        out.status = TaskStatus::Completed;
        out.product_path = op;
        out.logs.append("Done " + QString::number(out.duration_ms) + "ms");
    } else {
        out.status = TaskStatus::Failed;
        if (!out.error_message) out.error_message = "Failed";
        if (out.error_code == ConversionError::None) out.error_code = ConversionError::Unknown;
    }

    queue_->finishTask(handle, out);
}

} // namespace conversion
} // namespace dmc
