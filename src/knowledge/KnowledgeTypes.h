// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — KnowledgeTypes
// 知识库领域类型：Document、Chunk、Citation、RetrievalResult。
//
// 注意：模块 B 提供的 StructBlock / SourceSpan 使用 std::string，
// 这里定义模块 C 自己的 QString 版本，由 IngestionService 在边界转换。
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <QString>
#include <QList>
#include <QVector>
#include <QDateTime>
#include <QVariant>

namespace dmc::knowledge {

enum class SourceFormat {
    Markdown,
    Docx,
    Pdf,
    Html,
    Unknown,
};

// ─── 结构块（与模块 B 的 StructBlock 对应） ─────────────────────────────────
enum class BlockType {
    Heading,
    Paragraph,
    ListItem,
    CodeBlock,
    TableCell,
    Blockquote,
};

struct StructBlock {
    BlockType type       = BlockType::Paragraph;
    int       level      = 0;          // 标题级别 1-6
    QString   text;
    int       sourceLine = -1;         // 源文档行号
    int       sourcePage = -1;         // PDF 页码；DOCX/MD/HTML 为 -1
};

// 来源定位（与模块 B 的 SourceSpan 对应）
struct SourceSpan {
    int     page      = -1;
    int     lineStart = -1;
    int     charStart = -1;
    QString anchor;                   // HTML 锚点 / MD 标题 slug
};

// ─── 知识库文档记录 ───────────────────────────────────────────────────────────

enum class IngestionStatus {
    Pending,
    Extracting,    // 文本提取中（调用模块 B）
    Chunking,      // 分块中
    Embedding,     // 嵌入中（调用 AIProvider）
    Ready,
    Failed,
};

struct KnowledgeDocument {
    qint64     id          = 0;        // SQLite 自增主键
    QString    title;                  // 文件名或自定义标题
    QString    sourcePath;             // 原始文件绝对路径
    QString    contentHash;            // SHA-256，用于去重和增量更新
    SourceFormat format = SourceFormat::Unknown;
    qint64     fileSize    = 0;
    int        chunkCount  = 0;
    int        pageCount   = -1;       // PDF 才有意义
    IngestionStatus status = IngestionStatus::Pending;
    QString    errorMessage;
    QDateTime  createdAt;
    QDateTime  updatedAt;
};

// ─── 分块 ─────────────────────────────────────────────────────────────────────

struct Chunk {
    qint64     id           = 0;
    qint64     documentId   = 0;
    QString    text;                    // 分块正文（已清洗）
    int        ordinal      = 0;         // 文档内顺序
    int        tokenEstimate = 0;        // 粗略 token 估算（4*chars/3）
    int        headingLevel = 0;         // 所属标题级别（0 = 无标题上下文）
    QString    headingPath;              // 如 "章一 > 1.1 简介"，用于引用展示

    // 来源定位（用于点击引用跳转）
    int        sourcePage   = -1;
    int        sourceLine   = -1;
    QString    anchor;

    // 嵌入向量（仅在内存中持有，落库到单独的 embeddings 表）
    QVector<float> embedding;
    QByteArray     embeddingBlob;       // BLOB 序列化形式
};

// ─── 检索结果 ─────────────────────────────────────────────────────────────────

struct Citation {
    qint64    documentId = 0;
    QString   documentTitle;
    QString   snippet;                  // 高亮命中的片段
    int       page       = -1;
    int       lineStart  = -1;
    QString   anchor;
    float     score      = 0.0f;        // 综合得分
};

struct RetrievalResult {
    QList<Citation> citations;
    QString         reconstructedContext;  // 拼好用于注入 LLM 的上下文文本
    bool            fromKeyword = false;   // 是否来自 FTS5
    bool            fromVector  = false;   // 是否来自向量
};

} // namespace dmc::knowledge
