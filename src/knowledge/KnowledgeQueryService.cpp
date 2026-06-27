// DocMind AI — KnowledgeQueryService 实现
//
// RRF (Reciprocal Rank Fusion) 合并 FTS5 与向量结果。
//   score = w_kw * 1/(rrfK + rank_kw) + w_vec * 1/(rrfK + rank_vec)
#include "knowledge/KnowledgeQueryService.h"
#include "utils/Logger.h"

#include <QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>
#include <QSet>
#include <QMap>
#include <algorithm>
#include <cmath>

namespace dmc::knowledge {

namespace {

float cosine(const QVector<float>& a, const QVector<float>& b) {
    if (a.size() != b.size() || a.isEmpty()) return 0.0f;
    float dot = 0.0f, na = 0.0f, nb = 0.0f;
    for (int i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        na  += a[i] * a[i];
        nb  += b[i] * b[i];
    }
    if (na <= 0.0f || nb <= 0.0f) return 0.0f;
    return dot / (std::sqrt(na) * std::sqrt(nb));
}

} // namespace

KnowledgeQueryService::KnowledgeQueryService(KnowledgeRepository* repo,
                                                ai::AIProvider* provider,
                                                QObject* parent)
    : QObject(parent), m_repo(repo), m_provider(provider) {}

RetrievalResult KnowledgeQueryService::retrieveByKeyword(const QString& query, int limit) {
    RetrievalResult out;
    if (!m_repo) return out;
    QList<Chunk> chunks = m_repo->searchByKeyword(query, limit);
    if (chunks.isEmpty()) return out;

    QList<qint64> ids;
    ids.reserve(chunks.size());
    for (const Chunk& c : chunks) ids.append(c.id);

    auto full = m_repo->loadChunksFull(ids);
    out.citations.reserve(full.size());
    for (int i = 0; i < full.size(); ++i) {
        Citation c;
        c.documentId    = full[i].chunk.documentId;
        c.documentTitle = full[i].documentTitle;
        c.snippet       = full[i].chunk.text;
        c.page          = full[i].chunk.sourcePage;
        c.lineStart      = full[i].chunk.sourceLine;
        c.anchor        = full[i].chunk.anchor;
        // 关键词排序本身就是 BM25 排序，越前分越高
        c.score = 1.0f / (1.0f + i);
        out.citations.append(c);
    }
    out.reconstructedContext = buildContext(
        [&]() { QList<Chunk> cs; for (const auto& f : full) cs << f.chunk; return cs; }(),
        [&]() { QList<QString> ts; for (const auto& f : full) ts << f.documentTitle; return ts; }(),
        4000);
    out.fromKeyword = true;
    return out;
}

QFuture<RetrievalResult> KnowledgeQueryService::retrieve(const QString& query,
                                                            const RetrievalOptions& opts) {
    auto iface = std::make_shared<QFutureInterface<RetrievalResult>>();
    iface->reportStarted();

    QPointer<KnowledgeQueryService> self(this);
    KnowledgeRepository* repo = m_repo;
    ai::AIProvider* provider = m_provider;

    // 无 provider 或无 embedding 数据 -> 退化为关键词检索
    if (!provider || !repo) {
        RetrievalResult r = retrieveByKeyword(query, opts.topK);
        iface->reportResult(r);
        iface->reportFinished();
        return iface->future();
    }

    ai::EmbeddingRequest ereq;
    ereq.inputs.append(query);
    QFuture<ai::EmbeddingResult> ef = provider->embed(ereq);

    auto w = new QFutureWatcher<ai::EmbeddingResult>();
    QObject::connect(w, &QFutureWatcher<ai::EmbeddingResult>::finished, w,
        [self, w, query, opts, repo, iface]() {
            w->deleteLater();
            if (!self) { iface->reportFinished(); return; }

            ai::EmbeddingResult eres;
            try {
                eres = w->result();
            } catch (const std::exception& e) {
                LOG_WARN("KnowledgeQuery", QString("Embedding query failed, fallback to keyword: %1")
                                          .arg(QString::fromUtf8(e.what())));
                RetrievalResult r = self->retrieveByKeyword(query, opts.topK);
                iface->reportResult(r);
                iface->reportFinished();
                return;
            }
            if (eres.vectors.isEmpty()) {
                // 嵌入失败 -> 关键词降级
                RetrievalResult r = self->retrieveByKeyword(query, opts.topK);
                iface->reportResult(r);
                iface->reportFinished();
                return;
            }
            QVector<float> qvec = eres.vectors.first();

            // ── 关键词召回 ──────────────────────────────────────────────
            QList<Chunk> kw = repo->searchByKeyword(query, opts.keywordTopK);
            QMap<qint64, int> kwRank; // chunkId -> rank
            for (int i = 0; i < kw.size(); ++i) kwRank[kw[i].id] = i;

            // ── 向量召回 ────────────────────────────────────────────────
            QList<KnowledgeRepository::EmbeddingRow> rows = repo->allEmbeddings();
            QVector<QPair<qint64, float>> vecScores; // (chunkId, sim)
            vecScores.reserve(rows.size());
            for (const auto& r : rows) {
                float s = cosine(qvec, r.vector);
                vecScores.append({r.chunkId, s});
            }
            std::sort(vecScores.begin(), vecScores.end(),
                [](const auto& a, const auto& b) { return a.second > b.second; });
            QMap<qint64, int> vecRank;
            for (int i = 0; i < std::min(int(vecScores.size()), opts.vectorTopK); ++i) {
                vecRank[vecScores[i].first] = i;
            }

            // ── RRF 融合 ────────────────────────────────────────────────
            QSet<qint64> all;
            for (const Chunk& c : kw) all.insert(c.id);
            for (const auto& p : vecScores) {
                if (vecRank.contains(p.first)) all.insert(p.first);
            }

            QMap<qint64, float> fused;
            for (qint64 id : all) {
                float s = 0.0f;
                if (kwRank.contains(id))
                    s += opts.keywordWeight / float(opts.rrfK + kwRank[id] + 1);
                if (vecRank.contains(id))
                    s += opts.vectorWeight / float(opts.rrfK + vecRank[id] + 1);
                fused[id] = s;
            }

            QList<QPair<qint64, float>> sorted;
            for (auto it = fused.begin(); it != fused.end(); ++it)
                sorted.append({it.key(), it.value()});
            std::sort(sorted.begin(), sorted.end(),
                [](const auto& a, const auto& b) { return a.second > b.second; });

            int topK = std::min(opts.topK, int(sorted.size()));
            QList<qint64> topIds;
            for (int i = 0; i < topK; ++i) topIds.append(sorted[i].first);
            auto full = repo->loadChunksFull(topIds);

            RetrievalResult out;
            out.citations.reserve(full.size());
            QList<Chunk> chunks;
            QList<QString> titles;
            for (int i = 0; i < full.size(); ++i) {
                Citation c;
                c.documentId    = full[i].chunk.documentId;
                c.documentTitle = full[i].documentTitle;
                c.snippet       = full[i].chunk.text;
                c.page          = full[i].chunk.sourcePage;
                c.lineStart      = full[i].chunk.sourceLine;
                c.anchor        = full[i].chunk.anchor;
                qint64 id = full[i].chunk.id;
                c.score = fused.value(id, 0.0f);
                out.citations.append(c);
                chunks << full[i].chunk;
                titles << full[i].documentTitle;
            }
            out.reconstructedContext = buildContext(chunks, titles, opts.maxContextChars);
            out.fromKeyword = !kwRank.isEmpty();
            out.fromVector  = !vecRank.isEmpty();

            iface->reportResult(out);
            iface->reportFinished();
        });
    w->setFuture(ef);

    return iface->future();
}

QString KnowledgeQueryService::buildContext(const QList<Chunk>& chunks,
                                              const QList<QString>& docTitles,
                                              int maxChars) {
    QString out;
    int total = 0;
    for (int i = 0; i < chunks.size(); ++i) {
        const Chunk& c = chunks[i];
        QString title = i < docTitles.size() ? docTitles[i] : QStringLiteral("(unknown)");
        QString source;
        if (c.sourcePage >= 0) source = QStringLiteral("[第%1页]").arg(c.sourcePage);
        else if (!c.anchor.isEmpty()) source = QStringLiteral("[%1]").arg(c.anchor);

        QString block = QStringLiteral("--- 来源 %1: %2 %3 ---\n%4\n\n")
            .arg(i + 1).arg(title).arg(source).arg(c.text.trimmed());

        if (total + block.size() > maxChars) {
            // 截断尾部
            int remain = std::max(0, maxChars - total);
            out += block.left(remain);
            out += QStringLiteral("\n[... 截断 ...]");
            break;
        }
        out += block;
        total += block.size();
    }
    return out;
}

} // namespace dmc::knowledge
