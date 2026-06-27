// DocMind AI — KnowledgeRepository 实现
#include "knowledge/KnowledgeRepository.h"
#include "storage/DbMigrator.h"
#include "utils/Logger.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QVariant>
#include <QBuffer>
#include <QDataStream>
#include <QByteArray>
#include <QCryptographicHash>
#include <atomic>
#include <cstring>

namespace dmc::knowledge {

namespace {

// 把 QVector<float> 序列化为紧凑 BLOB（IEEE 754 小端，4 字节/分量）
QByteArray vectorToBlob(const QVector<float>& v) {
    QByteArray ba;
    ba.resize(int(v.size()) * int(sizeof(float)));
    if (!v.isEmpty()) {
        // 保证平台无关：用 memcpy + 不做字节序转换，依赖 float 本身为 IEEE 754
        std::memcpy(ba.data(), v.constData(), size_t(ba.size()));
    }
    return ba;
}

QVector<float> blobToVector(const QByteArray& ba, int dim) {
    QVector<float> v;
    if (dim <= 0 || ba.size() < dim * int(sizeof(float))) return v;
    v.resize(dim);
    std::memcpy(v.data(), ba.constData(), size_t(dim) * sizeof(float));
    return v;
}

QString statusToString(IngestionStatus s) {
    switch (s) {
        case IngestionStatus::Pending:    return QStringLiteral("Pending");
        case IngestionStatus::Extracting: return QStringLiteral("Extracting");
        case IngestionStatus::Chunking:   return QStringLiteral("Chunking");
        case IngestionStatus::Embedding:  return QStringLiteral("Embedding");
        case IngestionStatus::Ready:      return QStringLiteral("Ready");
        case IngestionStatus::Failed:     return QStringLiteral("Failed");
    }
    return QStringLiteral("Pending");
}

IngestionStatus statusFromString(const QString& s) {
    if (s == "Extracting") return IngestionStatus::Extracting;
    if (s == "Chunking")   return IngestionStatus::Chunking;
    if (s == "Embedding")  return IngestionStatus::Embedding;
    if (s == "Ready")      return IngestionStatus::Ready;
    if (s == "Failed")     return IngestionStatus::Failed;
    return IngestionStatus::Pending;
}

KnowledgeDocument hydrateDocument(const QSqlQuery& q) {
    KnowledgeDocument d;
    d.id          = q.value(QStringLiteral("id")).toLongLong();
    d.title       = q.value(QStringLiteral("title")).toString();
    d.sourcePath  = q.value(QStringLiteral("source_path")).toString();
    d.contentHash = q.value(QStringLiteral("content_hash")).toString();
    d.format      = static_cast<SourceFormat>(q.value(QStringLiteral("format")).toInt());
    d.fileSize    = q.value(QStringLiteral("file_size")).toLongLong();
    d.chunkCount  = q.value(QStringLiteral("chunk_count")).toInt();
    d.pageCount   = q.value(QStringLiteral("page_count")).toInt();
    d.status      = statusFromString(q.value(QStringLiteral("status")).toString());
    d.errorMessage= q.value(QStringLiteral("error_message")).toString();
    d.createdAt   = QDateTime::fromString(q.value(QStringLiteral("created_at")).toString(), Qt::ISODate);
    d.updatedAt   = QDateTime::fromString(q.value(QStringLiteral("updated_at")).toString(), Qt::ISODate);
    return d;
}

Chunk hydrateChunk(const QSqlQuery& q) {
    Chunk c;
    c.id           = q.value(QStringLiteral("id")).toLongLong();
    c.documentId   = q.value(QStringLiteral("document_id")).toLongLong();
    c.text         = q.value(QStringLiteral("text")).toString();
    c.ordinal      = q.value(QStringLiteral("ordinal")).toInt();
    c.tokenEstimate= q.value(QStringLiteral("token_estimate")).toInt();
    c.headingLevel = q.value(QStringLiteral("heading_level")).toInt();
    c.headingPath  = q.value(QStringLiteral("heading_path")).toString();
    c.sourcePage   = q.value(QStringLiteral("source_page")).toInt();
    c.sourceLine   = q.value(QStringLiteral("source_line")).toInt();
    c.anchor       = q.value(QStringLiteral("anchor")).toString();
    return c;
}

QString nowUtc() {
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}

} // namespace

KnowledgeRepository::KnowledgeRepository(const QString& dbPath, QObject* parent)
    : QObject(parent), m_dbPath(dbPath) {
    // 每实例独立连接名，避免多 Repository 共享连接冲突
    static std::atomic<int> seq{0};
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                      QStringLiteral("kb_%1").arg(seq.fetch_add(1)));
    m_db.setDatabaseName(m_dbPath);
}

KnowledgeRepository::~KnowledgeRepository() {
    close();
    QSqlDatabase::removeDatabase(m_db.connectionName());
}

bool KnowledgeRepository::open(QString* error) {
    if (!m_db.open()) {
        if (error) *error = m_db.lastError().text();
        LOG_ERROR("KnowledgeRepository", "DB open failed: " + m_db.lastError().text());
        return false;
    }
    // 启用外键级联
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("PRAGMA foreign_keys = ON"));

    storage::DbMigrator migrator(m_db);
    if (!migrator.migrate(error)) {
        LOG_ERROR("KnowledgeRepository", "Migration failed");
        return false;
    }
    LOG_INFO("KnowledgeRepository", "DB ready: " + m_dbPath);
    return true;
}

void KnowledgeRepository::close() {
    if (m_db.isOpen()) m_db.close();
}

qint64 KnowledgeRepository::insertDocument(const KnowledgeDocument& doc) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO documents "
        "(title, source_path, content_hash, format, file_size, chunk_count, page_count, "
        " status, error_message, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(doc.title);
    q.addBindValue(doc.sourcePath);
    q.addBindValue(doc.contentHash);
    q.addBindValue(int(doc.format));
    q.addBindValue(doc.fileSize);
    q.addBindValue(doc.chunkCount);
    q.addBindValue(doc.pageCount);
    q.addBindValue(statusToString(doc.status));
    q.addBindValue(doc.errorMessage);
    QString ts = nowUtc();
    q.addBindValue(ts);
    q.addBindValue(ts);

    if (!q.exec()) {
        LOG_ERROR("KnowledgeRepository", "insertDocument: " + q.lastError().text());
        return 0;
    }
    qint64 id = q.lastInsertId().toLongLong();
    LOG_INFO("KnowledgeRepository", QString("Inserted document id=%1 title=%2").arg(id).arg(doc.title));
    return id;
}

bool KnowledgeRepository::updateDocumentStatus(qint64 id, IngestionStatus s,
                                                const QString& errMsg, int chunkCount) {
    QSqlQuery q(m_db);
    QString sql = QStringLiteral(
        "UPDATE documents SET status = ?, error_message = ?, updated_at = ?");
    if (chunkCount >= 0) sql += QStringLiteral(", chunk_count = ?");
    sql += QStringLiteral(" WHERE id = ?");

    q.prepare(sql);
    q.addBindValue(statusToString(s));
    q.addBindValue(errMsg);
    q.addBindValue(nowUtc());
    if (chunkCount >= 0) q.addBindValue(chunkCount);
    q.addBindValue(id);
    if (!q.exec()) {
        LOG_ERROR("KnowledgeRepository", "updateDocumentStatus: " + q.lastError().text());
        return false;
    }
    return true;
}

std::optional<KnowledgeDocument> KnowledgeRepository::findDocumentById(qint64 id) const {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM documents WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec() || !q.next()) return std::nullopt;
    return hydrateDocument(q);
}

std::optional<KnowledgeDocument> KnowledgeRepository::findDocumentByHash(const QString& hash) const {
    if (hash.isEmpty()) return std::nullopt;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM documents WHERE content_hash = ? ORDER BY id DESC LIMIT 1"));
    q.addBindValue(hash);
    if (!q.exec() || !q.next()) return std::nullopt;
    return hydrateDocument(q);
}

QList<KnowledgeDocument> KnowledgeRepository::allDocuments() const {
    QList<KnowledgeDocument> out;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT * FROM documents ORDER BY updated_at DESC"));
    while (q.next()) out.append(hydrateDocument(q));
    return out;
}

bool KnowledgeRepository::deleteDocument(qint64 id) {
    // 外键 ON DELETE CASCADE 会清理 chunks；FTS 触发器同步移除；embedings 同样级联
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM documents WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec()) {
        LOG_ERROR("KnowledgeRepository", "deleteDocument: " + q.lastError().text());
        return false;
    }
    LOG_INFO("KnowledgeRepository", QString("Deleted document id=%1 (cascade)").arg(id));
    return true;
}

bool KnowledgeRepository::clearAll() {
    // 关闭外键约束以避免逐条级联性能问题；事务内全清
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("PRAGMA foreign_keys = OFF"))) return false;
    m_db.transaction();
    q.exec(QStringLiteral("DELETE FROM embeddings"));
    q.exec(QStringLiteral("DELETE FROM chunks"));
    // 清空 FTS（外部内容表用特殊语法）
    q.exec(QStringLiteral("INSERT INTO chunk_fts(chunk_fts) VALUES('rebuild')"));
    q.exec(QStringLiteral("DELETE FROM documents"));
    bool ok = m_db.commit();
    q.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
    LOG_INFO("KnowledgeRepository", "Cleared all documents/chunks/embeddings");
    return ok;
}

QList<qint64> KnowledgeRepository::insertChunks(qint64 documentId, const QList<Chunk>& chunks) {
    QList<qint64> ids;
    ids.reserve(chunks.size());
    if (!m_db.transaction()) {
        LOG_ERROR("KnowledgeRepository", "tx begin failed");
        return ids;
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO chunks "
        "(document_id, ordinal, text, token_estimate, heading_level, heading_path, "
        " source_page, source_line, anchor) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"));

    int ordinal = 0;
    for (const Chunk& c : chunks) {
        q.addBindValue(documentId);
        q.addBindValue(ordinal++);
        q.addBindValue(c.text);
        // 粗略 token 估算（如果调用方没给）
        int tok = c.tokenEstimate > 0 ? c.tokenEstimate : int(c.text.size() * 4 / 3);
        q.addBindValue(tok);
        q.addBindValue(c.headingLevel);
        q.addBindValue(c.headingPath);
        q.addBindValue(c.sourcePage);
        q.addBindValue(c.sourceLine);
        q.addBindValue(c.anchor);
        if (!q.exec()) {
            LOG_ERROR("KnowledgeRepository", "insertChunks: " + q.lastError().text());
            m_db.rollback();
            return {};
        }
        ids.append(q.lastInsertId().toLongLong());
    }
    if (!m_db.commit()) {
        LOG_ERROR("KnowledgeRepository", "tx commit failed");
        return {};
    }
    return ids;
}

bool KnowledgeRepository::setEmbedding(qint64 chunkId, const QVector<float>& vec,
                                         int dim, const QString& model) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO embeddings(chunk_id, vector_blob, dim, model, created_at) "
        "VALUES (?, ?, ?, ?, ?)"));
    q.addBindValue(chunkId);
    q.addBindValue(vectorToBlob(vec));
    q.addBindValue(dim);
    q.addBindValue(model);
    q.addBindValue(nowUtc());
    if (!q.exec()) {
        LOG_ERROR("KnowledgeRepository", "setEmbedding: " + q.lastError().text());
        return false;
    }
    return true;
}

QList<Chunk> KnowledgeRepository::searchByKeyword(const QString& query, int limit) const {
    QList<Chunk> out;
    if (query.trimmed().isEmpty()) return out;

    QSqlQuery q(m_db);
    // bm25() 可用则优先；否则降级 rank
    q.prepare(QStringLiteral(
        "SELECT c.id, c.document_id, c.ordinal, c.text, c.token_estimate, "
        "       c.heading_level, c.heading_path, c.source_page, c.source_line, c.anchor "
        "FROM chunk_fts f "
        "JOIN chunks c ON c.id = f.rowid "
        "WHERE chunk_fts MATCH ? "
        "ORDER BY bm25(chunk_fts) "
        "LIMIT ?"));
    q.addBindValue(query);
    q.addBindValue(limit);

    if (!q.exec()) {
        LOG_WARN("KnowledgeRepository", "FTS query failed, fallback rank: " + q.lastError().text());
        QSqlQuery q2(m_db);
        q2.prepare(QStringLiteral(
            "SELECT c.id, c.document_id, c.ordinal, c.text, c.token_estimate, "
            "       c.heading_level, c.heading_path, c.source_page, c.source_line, c.anchor "
            "FROM chunk_fts f JOIN chunks c ON c.id = f.rowid "
            "WHERE chunk_fts MATCH ? ORDER BY rank LIMIT ?"));
        q2.addBindValue(query);
        q2.addBindValue(limit);
        if (!q2.exec()) return out;
        while (q2.next()) out.append(hydrateChunk(q2));
        return out;
    }
    while (q.next()) out.append(hydrateChunk(q));
    return out;
}

QList<KnowledgeRepository::EmbeddingRow> KnowledgeRepository::allEmbeddings() const {
    QList<EmbeddingRow> out;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral(
        "SELECT e.chunk_id, c.document_id, e.vector_blob, e.dim "
        "FROM embeddings e JOIN chunks c ON c.id = e.chunk_id"));
    while (q.next()) {
        EmbeddingRow r;
        r.chunkId    = q.value(0).toLongLong();
        r.documentId = q.value(1).toLongLong();
        r.dim        = q.value(3).toInt();
        r.vector     = blobToVector(q.value(2).toByteArray(), r.dim);
        out.append(r);
    }
    return out;
}

std::optional<KnowledgeRepository::ChunkWithDoc>
KnowledgeRepository::loadChunkFull(qint64 chunkId) const {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT c.id, c.document_id, c.ordinal, c.text, c.token_estimate, "
        "       c.heading_level, c.heading_path, c.source_page, c.source_line, c.anchor, "
        "       d.title "
        "FROM chunks c JOIN documents d ON d.id = c.document_id "
        "WHERE c.id = ?"));
    q.addBindValue(chunkId);
    if (!q.exec() || !q.next()) return std::nullopt;
    ChunkWithDoc cw;
    cw.chunk = hydrateChunk(q);
    cw.documentTitle = q.value(10).toString();
    return cw;
}

QList<KnowledgeRepository::ChunkWithDoc>
KnowledgeRepository::loadChunksFull(const QList<qint64>& ids) const {
    QList<ChunkWithDoc> out;
    if (ids.isEmpty()) return out;
    QStringList ph;
    for (int i = 0; i < ids.size(); ++i) ph << QStringLiteral("?");
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT c.id, c.document_id, c.ordinal, c.text, c.token_estimate, "
        "       c.heading_level, c.heading_path, c.source_page, c.source_line, c.anchor, "
        "       d.title "
        "FROM chunks c JOIN documents d ON d.id = c.document_id "
        "WHERE c.id IN (%1)").arg(ph.join(QStringLiteral(", "))));
    for (qint64 id : ids) q.addBindValue(id);
    if (!q.exec()) return out;
    while (q.next()) {
        ChunkWithDoc cw;
        cw.chunk = hydrateChunk(q);
        cw.documentTitle = q.value(10).toString();
        out.append(cw);
    }
    return out;
}

bool KnowledgeRepository::rebuildFtsIndex() {
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("INSERT INTO chunk_fts(chunk_fts) VALUES('rebuild')"))) {
        LOG_ERROR("KnowledgeRepository", "rebuildFtsIndex: " + q.lastError().text());
        return false;
    }
    LOG_INFO("KnowledgeRepository", "FTS index rebuilt");
    return true;
}

} // namespace dmc::knowledge
