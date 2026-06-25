// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — ChunkingStrategy
// 分块策略抽象：按标题 + 长度切分 StructBlock 流为 Chunk。
//
// 默认实现 TitleLengthChunker：
//   - 维护当前标题路径栈（h1 > h2 > h3）
//   - 累积正文块直到接近 maxChars，遇到新标题或长度上限则切出
//   - 单块超长时按段落硬切，保留 headingPath
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include "knowledge/KnowledgeTypes.h"

#include <QList>
#include <QString>

namespace dmc::knowledge {

struct ChunkingOptions {
    int targetChars = 800;     // 目标长度（字符）
    int maxChars    = 1200;    // 硬上限，超出则切分
    int overlapChars = 100;     // 块之间重叠（首期可设 0）
    bool keepHeadingsAsContext = true;
};

class ChunkingStrategy {
public:
    virtual ~ChunkingStrategy() = default;
    virtual QList<Chunk> chunk(const QList<StructBlock>& blocks,
                                const ChunkingOptions& opts) const = 0;
};

class TitleLengthChunker : public ChunkingStrategy {
public:
    QList<Chunk> chunk(const QList<StructBlock>& blocks,
                       const ChunkingOptions& opts) const override;
};

} // namespace dmc::knowledge
