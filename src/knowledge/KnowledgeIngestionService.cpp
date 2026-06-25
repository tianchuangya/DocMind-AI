// DocMind AI — KnowledgeIngestionService 实现
//
// 编排：提取（同步，worker 线程）→ 分块 → 嵌入（异步）→ 存储。
// 批量：串行执行以便错误隔离；每文件完成发 fileIngested / fileFailed。
#include "knowledge/KnowledgeIngestionService.h"
#include "knowledge/ChunkingStrategy.h"
#include "knowledge/KnowledgeRepository.h"
#include "utils/Logger.h"

#include <QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QFile>
#include <QPointer>
#include <atomic>
#include <functional>
#include <memory>

namespace dmc::knowledge {

namespace {

SourceFormat formatFromString(const QString& s) {
    QString l = s.toLower();
    if (l == "md" || l == "markdown") return SourceFormat::Markdown;
    if (l == "docx") return SourceFormat::Docx;
    if (l == "pdf") return SourceFormat::Pdf;
    if (l == "html" || l == "htm") return SourceFormat::Html;
    return SourceFormat::Unknown;
}

// 把 ExtractionOutput 的 spans 与 blocks 对齐到 chunks（按文本顺序）。
// 简化：取每个 chunk 的代表 block 的 span 信息（已在 chunking 时填充）。
void applySpansToChunks(QList<Chunk>& chunks, const QList<SourceSpan>& /*spans*/) {
    // 首期：chunking 时已通过 StructBlock 携带 sourcePage/Line，
    // 此处 span 仅作 anchor 来源用，简单留空。
    // 后续可按 chunk.ordinal 与 spans 顺序对齐。
}

} // namespace

KnowledgeIngestionService::KnowledgeIngestionService(KnowledgeRepository* repo,
                                                       ai::AIProvider* provider,
                                                       ExtractionAdapter* extractor,
                                                       QObject* parent)
    : QObject(parent), m_repo(repo), m_provider(provider), m_extractor(extractor) {}

void KnowledgeIngestionService::setChunkingOptions(const ChunkingOptions& opts) {
    m_chunkOpts = opts;
}

QFuture<IngestionResult> KnowledgeIngestionService::ingest(const IngestionRequest& req) {
    auto iface = std::make_shared<QFutureInterface<IngestionResult>>();
    iface->reportStarted();

    QPointer<KnowledgeIngestionService> self(this);
    KnowledgeRepository* repo = m_repo;
    ai::AIProvider* provider = m_provider;
    ExtractionAdapter* extractor = m_extractor;
    ChunkingOptions opts = m_chunkOpts;

    QtConcurrent::run([=]() {
        IngestionResult result;
        result.userTag = req.userTag;

        // ── 1. 计算内容哈希（去重） ────────────────────────────────────────
        QString hashInput;
        qint64  fileSize = 0;
        if (!req.sourcePath.isEmpty() && req.sourceContent.isEmpty()) {
            QFile f(req.sourcePath);
            if (f.open(QIODevice::ReadOnly)) {
                fileSize = f.size();
                QCryptographicHash h(QCryptographicHash::Sha256);
                if (h.addData(&f)) hashInput = h.result().toHex();
            }
        } else if (!req.sourceContent.isEmpty()) {
            hashInput = QString::fromUtf8(
                QCryptographicHash::hash(req.sourceContent.toUtf8(), QCryptographicHash::Sha256).toHex());
        }

        if (hashInput.isEmpty() && req.sourceContent.isEmpty()) {
            result.errorMessage = QStringLiteral("Source missing");
            QMetaObject::invokeMethod(self.data(), [self, result]() {
                if (self) emit self->fileFailed(QString(), result.errorMessage);
            }, Qt::QueuedConnection);
            iface->reportResult(result);
            iface->reportFinished();
            return;
        }

        // ── 2. 文档已存在（同 hash）则跳过 ─────────────────────────────────
        if (!hashInput.isEmpty() && repo) {
            auto existing = repo->findDocumentByHash(hashInput);
            if (existing && existing->status == IngestionStatus::Ready) {
                result.ok = true;
                result.documentId = existing->id;
                result.chunkCount = existing->chunkCount;
                result.pageCount  = existing->pageCount;
                QMetaObject::invokeMethod(self.data(), [self, result]() {
                    if (self) emit self->fileIngested(result);
                }, Qt::QueuedConnection);
                iface->reportResult(result);
                iface->reportFinished();
                return;
            }
        }

        // ── 3. 文档落库（占位状态） ─────────────────────────────────────────
        KnowledgeDocument doc;
        doc.title = req.title.isEmpty()
                    ? QFileInfo(req.sourcePath).fileName()
                    : req.title;
        if (doc.title.isEmpty()) doc.title = QStringLiteral("Untitled");
        doc.sourcePath  = req.sourcePath;
        doc.contentHash = hashInput;
        doc.format      = formatFromString(req.sourceFormat);
        doc.fileSize    = fileSize;
        doc.status      = IngestionStatus::Extracting;
        qint64 docId = repo ? repo->insertDocument(doc) : 0;
        if (docId == 0) {
            result.errorMessage = QStringLiteral("Failed to insert document");
            iface->reportResult(result);
            iface->reportFinished();
            return;
        }

        // ── 4. 提取 ─────────────────────────────────────────────────────────
        ExtractionInput ein;
        ein.sourcePath     = req.sourcePath;
        ein.sourceContent  = req.sourceContent;
        ein.sourceFormat   = req.sourceFormat;
        ein.preferStructure= true;
        ExtractionOutput eout = extractor ? extractor->extract(ein)
                                            : ExtractionOutput{};
        if (!extractor && !req.sourceContent.isEmpty()) {
            eout.ok = true;
            eout.plainText = req.sourceContent;
            eout.markdownText = req.sourceContent;
            eout.blocks = {
                StructBlock{BlockType::Paragraph, 0, req.sourceContent, 1, -1}
            };
        }
        if (!eout.ok) {
            if (repo) repo->updateDocumentStatus(docId, IngestionStatus::Failed,
                                                  eout.errorMessage, 0);
            result.errorMessage = eout.errorMessage.isEmpty()
                                  ? QStringLiteral("Extraction failed")
                                  : eout.errorMessage;
            QMetaObject::invokeMethod(self.data(), [self, req, result]() {
                if (self) emit self->fileFailed(req.sourcePath, result.errorMessage);
            }, Qt::QueuedConnection);
            iface->reportResult(result);
            iface->reportFinished();
            return;
        }

        // ── 5. 分块 ────────────────────────────────────────────────────────
        if (repo) repo->updateDocumentStatus(docId, IngestionStatus::Chunking, {}, 0);
        TitleLengthChunker chunker;
        QList<Chunk> chunks = chunker.chunk(eout.blocks, opts);
        applySpansToChunks(chunks, eout.spans);

        QList<qint64> chunkIds = repo ? repo->insertChunks(docId, chunks) : QList<qint64>{};
        if (chunkIds.isEmpty() && !chunks.isEmpty()) {
            if (repo) repo->updateDocumentStatus(docId, IngestionStatus::Failed,
                                                  QStringLiteral("Chunk insert failed"), 0);
            result.errorMessage = QStringLiteral("Chunk insert failed");
            iface->reportResult(result);
            iface->reportFinished();
            return;
        }

        // ── 6. 嵌入（批量） ───────────────────────────────────────────────
        if (repo) repo->updateDocumentStatus(docId, IngestionStatus::Embedding, {}, int(chunkIds.size()));

        if (provider && !chunkIds.isEmpty()) {
            // 把每个 chunk 文本拼到 embedding 请求；首期一次性批量
            ai::EmbeddingRequest ereq;
            ereq.inputs.reserve(chunks.size());
            for (const Chunk& c : chunks) {
                ereq.inputs.append(c.text);
            }
            // 同步等待 future（已在 worker 线程）
            QFuture<ai::EmbeddingResult> ef = provider->embed(ereq);
            ai::EmbeddingResult eres = ef.result();

            if (eres.vectors.size() == chunks.size()) {
                int dim = eres.vectors.isEmpty() ? 0 : eres.vectors.first().size();
                for (int i = 0; i < chunkIds.size(); ++i) {
                    repo->setEmbedding(chunkIds[i], eres.vectors[i], dim,
                                        ereq.model.isEmpty() ? QStringLiteral("default") : ereq.model);
                }
            } else {
                LOG_WARN("Ingestion", QString("Embedding count mismatch: req=%1 got=%2")
                                        .arg(chunks.size()).arg(eres.vectors.size()));
            }
        }

        // ── 7. 完成 ────────────────────────────────────────────────────────
        if (repo) repo->updateDocumentStatus(docId, IngestionStatus::Ready, {},
                                              int(chunkIds.size()));

        // 统计 PDF 页数（粗略：取最大 sourcePage）
        int maxPage = -1;
        for (const Chunk& c : chunks) maxPage = std::max(maxPage, c.sourcePage);

        result.ok = true;
        result.documentId = docId;
        result.chunkCount = int(chunkIds.size());
        result.pageCount   = maxPage;

        QMetaObject::invokeMethod(self.data(), [self, result]() {
            if (self) emit self->fileIngested(result);
        }, Qt::QueuedConnection);

        iface->reportResult(result);
        iface->reportFinished();
    });

    return iface->future();
}

void KnowledgeIngestionService::ingestBatch(const QList<IngestionRequest>& reqs) {
    QPointer<KnowledgeIngestionService> self(this);
    int total = reqs.size();
    std::shared_ptr<std::atomic<int>> done = std::make_shared<std::atomic<int>>(0);

    if (reqs.isEmpty()) {
        emit batchFinished();
        return;
    }
    emit batchProgress(0, total, reqs[0].sourcePath);

    // 用 std::function 包装递归 lambda：每完成一个发进度，下一个再启动
    auto launchNext = std::make_shared<std::function<void(int)>>();
    *launchNext = [self, reqs, total, done, this, launchNext](int idx) {
        if (!self || idx >= total) {
            if (self) emit self->batchFinished();
            return;
        }
        QFuture<IngestionResult> f = ingest(reqs[idx]);
        QFutureWatcher<IngestionResult>* w = new QFutureWatcher<IngestionResult>();
        QObject::connect(w, &QFutureWatcher<IngestionResult>::finished, self.data(),
            [self, w, done, total, idx, reqs, launchNext]() {
                w->deleteLater();
                done->fetch_add(1);
                if (!self) return;
                if (done->load() >= total) {
                    emit self->batchFinished();
                } else {
                    int next = idx + 1;
                    emit self->batchProgress(done->load(), total,
                                              next < total ? reqs[next].sourcePath : QString());
                    (*launchNext)(next);
                }
            });
        w->setFuture(f);
    };
    (*launchNext)(0);
}

void KnowledgeIngestionService::cancelDocument(qint64 documentId) {
    // 首期：标记失败，下次轮询跳过；细粒度取消后续完善
    if (m_repo) m_repo->updateDocumentStatus(documentId, IngestionStatus::Failed,
                                                QStringLiteral("Cancelled by user"), 0);
    LOG_INFO("Ingestion", QString("Cancel requested for doc=%1").arg(documentId));
}

void KnowledgeIngestionService::cancelAll() {
    if (!m_repo) return;
    auto docs = m_repo->allDocuments();
    for (const auto& d : docs) {
        if (d.status == IngestionStatus::Extracting ||
            d.status == IngestionStatus::Chunking ||
            d.status == IngestionStatus::Embedding) {
            cancelDocument(d.id);
        }
    }
}

} // namespace dmc::knowledge
