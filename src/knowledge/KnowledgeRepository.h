// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — KnowledgeRepository
// 知识库持久化层：SQLite。
//
// 表结构（schema 由 DbMigrator 管理）：
//   documents(id, title, source_path, content_hash, format, file_size,
//             chunk_count, page_count, status, error_message, created_at, updated_at)
//   chunks(id, document_id, ordinal, text, token_estimate,
//          heading_level, heading_path, source_page, source_line, anchor)
//   chunk_fts(rowid, text)            -- FTS5 虚拟表，与 chunks 一一对应
//   embeddings(chunk_id, vector_blob, dim, model, created_at)
//   kb_settings(key, value)
//
// 线程：所有方法同步执行，假设在 worker 线程或 QtConcurrent 中调用。
//       调用方（IngestionService）负责线程切分。
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include "knowledge/KnowledgeTypes.h"

#include <QObject>
#include <QString>
#include <QList>
#include <QSqlDatabase>
#include <optional>

namespace dmc::knowledge {

class KnowledgeRepository : public QObject {
    Q_OBJECT
public:
    explicit KnowledgeRepository(const QString& dbPath, QObject* parent = nullptr);
    ~KnowledgeRepository() override;

    bool open(QString* error = nullptr);
    void close();

    // ─── 文档 CRUD ────────────────────────────────────────────────────────────
    qint64                   insertDocument(const KnowledgeDocument& doc);
    bool                     updateDocumentStatus(qint64 id, IngestionStatus s,
                                                   const QString& errMsg,
                                                   int chunkCount = -1);
    std::optional<KnowledgeDocument> findDocumentById(qint64 id) const;
    std::optional<KnowledgeDocument> findDocumentByHash(const QString& hash) const;
    QList<KnowledgeDocument> allDocuments() const;
    bool                     deleteDocument(qint64 id);    // 级联删除 chunks + embeddings + FTS
    bool                     clearAll();                  // 清空全部知识库

    // ─── 分块 CRUD ────────────────────────────────────────────────────────────
    // 批量插入分块（事务），返回插入的 chunk id 列表
    QList<qint64> insertChunks(qint64 documentId, const QList<Chunk>& chunks);

    // 更新某分块的嵌入向量（编码为 float 数组的 BLOB）
    bool setEmbedding(qint64 chunkId, const QVector<float>& vec,
                      int dim, const QString& model);

    // ─── 查询 ────────────────────────────────────────────────────────────────
    // FTS5 关键词召回
    QList<Chunk> searchByKeyword(const QString& query, int limit = 10) const;

    // 全量向量（用于把用户 query 向量与所有 chunk 向量做余弦相似度）
    // 大库应改为 ANN 索引；首期 SQLite 内遍历可接受（< 10万 chunk）
    struct EmbeddingRow {
        qint64          chunkId;
        qint64          documentId;
        QVector<float>  vector;
        int             dim;
    };
    QList<EmbeddingRow> allEmbeddings() const;

    // 取 chunk 文本（带文档信息，构造 Citation）
    struct ChunkWithDoc {
        Chunk    chunk;
        QString  documentTitle;
    };
    std::optional<ChunkWithDoc> loadChunkFull(qint64 chunkId) const;
    QList<ChunkWithDoc>         loadChunksFull(const QList<qint64>& ids) const;

    // ─── 重建索引 ─────────────────────────────────────────────────────────────
    bool rebuildFtsIndex();

private:
    QString      m_dbPath;
    QSqlDatabase m_db;
};

} // namespace dmc::knowledge
