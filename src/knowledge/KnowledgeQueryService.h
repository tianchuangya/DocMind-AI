// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — KnowledgeQueryService
// 知识库检索：FTS5 关键词召回 + 向量余弦相似度融合排序。
//
// 流程：
//   1. 用户 query → 嵌入向量（AIProvider::embed）
//   2. FTS5 关键词召回 topK_kw
//   3. 全量向量余弦相似度，取 topK_vec
//   4. RRF（Reciprocal Rank Fusion）合并两路结果，输出 topK
//   5. 拼装 retrievedContext 文本（带来源标识），供 LLM 注入
//
// 安全：仅发送检索命中的最小上下文 + 来源标识给模型，
//       不发送 API Key、不发送知识库原文未命中部分。
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include "knowledge/KnowledgeTypes.h"
#include "knowledge/KnowledgeRepository.h"

#include <QObject>
#include <QString>
#include <QList>
#include <QFuture>

namespace dmc::ai       { class AIProvider; }
namespace dmc::knowledge { class KnowledgeRepository; }

namespace dmc::knowledge {

struct RetrievalOptions {
    int   topK            = 5;        // 最终返回数量
    int   keywordTopK     = 10;       // FTS5 召回数
    int   vectorTopK      = 10;       // 向量召回数
    float keywordWeight   = 0.4f;     // RRF 融合权重
    float vectorWeight    = 0.6f;
    int   rrfK            = 60;       // RRF 常数
    int   maxContextChars  = 4000;     // 拼装上下文上限
};

class KnowledgeQueryService : public QObject {
    Q_OBJECT
public:
    KnowledgeQueryService(KnowledgeRepository* repo,
                           ai::AIProvider* provider,
                           QObject* parent = nullptr);

    // 异步：先 embed query，再融合检索
    QFuture<RetrievalResult> retrieve(const QString& query,
                                       const RetrievalOptions& opts = {});

    // 仅关键词检索（不调嵌入，离线/降级用）
    RetrievalResult retrieveByKeyword(const QString& query, int limit = 5);

    // 把命中的 chunks 拼成上下文文本
    static QString buildContext(const QList<Chunk>& chunks,
                                 const QList<QString>& docTitles,
                                 int maxChars);

private:
    KnowledgeRepository* m_repo;
    ai::AIProvider*      m_provider;
};

} // namespace dmc::knowledge
