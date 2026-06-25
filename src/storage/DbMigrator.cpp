// DocMind AI — DbMigrator 实现
// SQLite schema 版本化迁移。
#include "storage/DbMigrator.h"

#include "utils/Logger.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QVariant>

namespace dmc::storage {

namespace {
constexpr int kLatestVersion = 1;
}

DbMigrator::DbMigrator(const QSqlDatabase& db, QObject* parent)
    : QObject(parent), m_db(db) {}

int DbMigrator::currentVersion() const {
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT version FROM schema_version ORDER BY version DESC LIMIT 1"))) {
        // 表不存在视为版本 0
        return 0;
    }
    if (q.next()) return q.value(0).toInt();
    return 0;
}

int DbMigrator::latestVersion() { return kLatestVersion; }

bool DbMigrator::migrate(QString* error) {
    if (!m_db.isOpen()) {
        if (error) *error = QStringLiteral("Database not open");
        return false;
    }

    // 确保 schema_version 表存在
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS schema_version ("
            "  version INTEGER PRIMARY KEY,"
            "  applied_at TEXT NOT NULL)"))) {
        if (error) *error = q.lastError().text();
        return false;
    }

    int v = currentVersion();
    while (v < kLatestVersion) {
        bool ok = false;
        if (v + 1 == 1) ok = applyV1(error);
        // 后续版本在此追加 else if 分支
        if (!ok) return false;
        ++v;
    }
    LOG_INFO("DbMigrator", QString("Migration complete at version %1").arg(v));
    return true;
}

bool DbMigrator::applyV1(QString* error) {
    QSqlQuery q(m_db);

    // 应用设置（非敏感）
    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS app_settings ("
            "  key   TEXT PRIMARY KEY,"
            "  value TEXT)"))) {
        if (error) *error = q.lastError().text();
        return false;
    }

    // AI Provider 配置（不含 API Key）
    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS ai_providers ("
            "  id                INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  display_name      TEXT NOT NULL,"
            "  base_url          TEXT NOT NULL,"
            "  embedding_base_url TEXT,"
            "  api_key_ref       TEXT,"
            "  chat_model        TEXT,"
            "  embedding_model   TEXT,"
            "  request_timeout_ms INTEGER DEFAULT 60000,"
            "  proxy_url         TEXT,"
            "  is_default        INTEGER DEFAULT 0,"
            "  created_at        TEXT NOT NULL,"
            "  updated_at        TEXT NOT NULL)"))) {
        if (error) *error = q.lastError().text();
        return false;
    }

    // 兼容已经创建过的 v1 数据库：补充向量服务独立 Base URL。
    if (!q.exec(QStringLiteral(
            "ALTER TABLE ai_providers ADD COLUMN embedding_base_url TEXT"))) {
        const QString msg = q.lastError().text().toLower();
        if (!msg.contains(QStringLiteral("duplicate column"))) {
            if (error) *error = q.lastError().text();
            return false;
        }
    }

    // 知识库文档
    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS documents ("
            "  id            INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  title         TEXT NOT NULL,"
            "  source_path   TEXT,"
            "  content_hash  TEXT,"
            "  format        INTEGER NOT NULL DEFAULT 0,"
            "  file_size     INTEGER DEFAULT 0,"
            "  chunk_count   INTEGER DEFAULT 0,"
            "  page_count    INTEGER DEFAULT -1,"
            "  status        INTEGER NOT NULL DEFAULT 0,"
            "  error_message TEXT,"
            "  created_at    TEXT NOT NULL,"
            "  updated_at    TEXT NOT NULL)"))) {
        if (error) *error = q.lastError().text();
        return false;
    }

    // 内容哈希索引（用于去重检测）
    if (!q.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_documents_hash "
            "ON documents(content_hash)"))) {
        if (error) *error = q.lastError().text();
        return false;
    }

    // 分块
    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS chunks ("
            "  id             INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  document_id    INTEGER NOT NULL,"
            "  ordinal        INTEGER NOT NULL,"
            "  text           TEXT NOT NULL,"
            "  token_estimate INTEGER DEFAULT 0,"
            "  heading_level  INTEGER DEFAULT 0,"
            "  heading_path   TEXT,"
            "  source_page    INTEGER DEFAULT -1,"
            "  source_line    INTEGER DEFAULT -1,"
            "  anchor         TEXT,"
            "  FOREIGN KEY(document_id) REFERENCES documents(id) ON DELETE CASCADE)"))) {
        if (error) *error = q.lastError().text();
        return false;
    }

    if (!q.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_chunks_doc "
            "ON chunks(document_id)"))) {
        if (error) *error = q.lastError().text();
        return false;
    }

    // FTS5 全文索引（外部内容表，与 chunks.text 同步）
    // 用外接 rowid 关联，避免数据双份存储
    if (!q.exec(QStringLiteral(
            "CREATE VIRTUAL TABLE IF NOT EXISTS chunk_fts "
            "USING fts5(text, content='chunks', content_rowid='id')"))) {
        if (error) *error = q.lastError().text();
        return false;
    }

    // FTS 同步触发器：插入/删除/更新 chunks 时维护 chunk_fts
    if (!q.exec(QStringLiteral(
            "CREATE TRIGGER IF NOT EXISTS chunks_ai AFTER INSERT ON chunks BEGIN "
            "  INSERT INTO chunk_fts(rowid, text) VALUES (new.id, new.text); "
            "END"))) {
        if (error) *error = q.lastError().text();
        return false;
    }
    if (!q.exec(QStringLiteral(
            "CREATE TRIGGER IF NOT EXISTS chunks_ad AFTER DELETE ON chunks BEGIN "
            "  INSERT INTO chunk_fts(chunk_fts, rowid, text) VALUES ('delete', old.id, old.text); "
            "END"))) {
        if (error) *error = q.lastError().text();
        return false;
    }
    if (!q.exec(QStringLiteral(
            "CREATE TRIGGER IF NOT EXISTS chunks_au AFTER UPDATE ON chunks BEGIN "
            "  INSERT INTO chunk_fts(chunk_fts, rowid, text) VALUES ('delete', old.id, old.text); "
            "  INSERT INTO chunk_fts(rowid, text) VALUES (new.id, new.text); "
            "END"))) {
        if (error) *error = q.lastError().text();
        return false;
    }

    // 嵌入向量（独立表，便于将来替换为 ANN 索引）
    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS embeddings ("
            "  chunk_id    INTEGER PRIMARY KEY,"
            "  vector_blob BLOB NOT NULL,"
            "  dim         INTEGER NOT NULL,"
            "  model       TEXT,"
            "  created_at  TEXT NOT NULL,"
            "  FOREIGN KEY(chunk_id) REFERENCES chunks(id) ON DELETE CASCADE)"))) {
        if (error) *error = q.lastError().text();
        return false;
    }

    if (!q.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_embeddings_chunk "
            "ON embeddings(chunk_id)"))) {
        if (error) *error = q.lastError().text();
        return false;
    }

    // 记录版本
    QSqlQuery vq(m_db);
    vq.prepare(QStringLiteral(
        "INSERT INTO schema_version(version, applied_at) VALUES (?, ?)"));
    vq.addBindValue(kLatestVersion);
    vq.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    if (!vq.exec()) {
        if (error) *error = vq.lastError().text();
        return false;
    }
    return true;
}

bool DbMigrator::applyV2(QString* error) {
    // 预留：例如增加 ANN 向量索引表
    Q_UNUSED(error);
    return true;
}

} // namespace dmc::storage
