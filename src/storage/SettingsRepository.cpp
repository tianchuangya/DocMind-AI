// DocMind AI — SettingsRepository 实现
#include "storage/SettingsRepository.h"
#include "storage/DbMigrator.h"
#include "utils/Logger.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QVariant>
#include <atomic>

namespace dmc::storage {

namespace {

ProviderSettings hydrate(const QSqlQuery& q) {
    ProviderSettings s;
    s.id              = q.value(QStringLiteral("id")).toLongLong();
    s.displayName     = q.value(QStringLiteral("display_name")).toString();
    s.baseUrl         = q.value(QStringLiteral("base_url")).toString();
    s.apiKeyRef       = q.value(QStringLiteral("api_key_ref")).toString();
    s.chatModel       = q.value(QStringLiteral("chat_model")).toString();
    s.embeddingModel  = q.value(QStringLiteral("embedding_model")).toString();
    s.requestTimeoutMs= q.value(QStringLiteral("request_timeout_ms")).toInt();
    s.proxyUrl        = q.value(QStringLiteral("proxy_url")).toString();
    s.isDefault       = q.value(QStringLiteral("is_default")).toBool();
    return s;
}

QString nowUtc() { return QDateTime::currentDateTimeUtc().toString(Qt::ISODate); }

} // namespace

SettingsRepository::SettingsRepository(const QString& dbPath,
                                         SecureCredentialStore* creds,
                                         QObject* parent)
    : QObject(parent), m_dbPath(dbPath), m_creds(creds) {
    static std::atomic<int> seq{0};
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                      QStringLiteral("settings_%1").arg(seq.fetch_add(1)));
    m_db.setDatabaseName(m_dbPath);
}

SettingsRepository::~SettingsRepository() {
    if (m_db.isOpen()) m_db.close();
    QSqlDatabase::removeDatabase(m_db.connectionName());
}

bool SettingsRepository::open(QString* error) {
    if (!m_db.open()) {
        if (error) *error = m_db.lastError().text();
        return false;
    }
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("PRAGMA foreign_keys = ON"));

    DbMigrator migrator(m_db);
    if (!migrator.migrate(error)) {
        LOG_ERROR("SettingsRepository", "Migration failed");
        return false;
    }
    LOG_INFO("SettingsRepository", "DB ready: " + m_dbPath);
    return true;
}

void SettingsRepository::close() {
    if (m_db.isOpen()) m_db.close();
}

qint64 SettingsRepository::upsertProvider(const ProviderSettings& s) {
    QSqlQuery q(m_db);
    QString ts = nowUtc();
    if (s.id > 0) {
        q.prepare(QStringLiteral(
            "UPDATE ai_providers SET display_name=?, base_url=?, api_key_ref=?, "
            "  chat_model=?, embedding_model=?, request_timeout_ms=?, proxy_url=?, "
            "  updated_at=? WHERE id=?"));
        q.addBindValue(s.displayName);
        q.addBindValue(s.baseUrl);
        q.addBindValue(s.apiKeyRef);
        q.addBindValue(s.chatModel);
        q.addBindValue(s.embeddingModel);
        q.addBindValue(s.requestTimeoutMs);
        q.addBindValue(s.proxyUrl);
        q.addBindValue(ts);
        q.addBindValue(s.id);
        if (!q.exec()) {
            LOG_ERROR("SettingsRepository", "upsert update: " + q.lastError().text());
            return 0;
        }
        if (s.isDefault) setDefaultProvider(s.id);
        return s.id;
    }

    q.prepare(QStringLiteral(
        "INSERT INTO ai_providers "
        "(display_name, base_url, api_key_ref, chat_model, embedding_model, "
        " request_timeout_ms, proxy_url, is_default, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(s.displayName);
    q.addBindValue(s.baseUrl);
    q.addBindValue(s.apiKeyRef);
    q.addBindValue(s.chatModel);
    q.addBindValue(s.embeddingModel);
    q.addBindValue(s.requestTimeoutMs);
    q.addBindValue(s.proxyUrl);
    q.addBindValue(s.isDefault ? 1 : 0);
    q.addBindValue(ts);
    q.addBindValue(ts);
    if (!q.exec()) {
        LOG_ERROR("SettingsRepository", "upsert insert: " + q.lastError().text());
        return 0;
    }
    qint64 newId = q.lastInsertId().toLongLong();
    if (s.isDefault) setDefaultProvider(newId);
    LOG_INFO("SettingsRepository", QString("Inserted provider id=%1").arg(newId));
    return newId;
}

std::optional<ProviderSettings> SettingsRepository::providerById(qint64 id) const {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM ai_providers WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec() || !q.next()) return std::nullopt;
    return hydrate(q);
}

std::optional<ProviderSettings> SettingsRepository::defaultProvider() const {
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT * FROM ai_providers WHERE is_default = 1 LIMIT 1"));
    if (!q.next()) {
        // 降级：取第一条
        QSqlQuery q2(m_db);
        q2.exec(QStringLiteral("SELECT * FROM ai_providers ORDER BY id ASC LIMIT 1"));
        if (!q2.next()) return std::nullopt;
        return hydrate(q2);
    }
    return hydrate(q);
}

QList<ProviderSettings> SettingsRepository::allProviders() const {
    QList<ProviderSettings> out;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT * FROM ai_providers ORDER BY is_default DESC, id ASC"));
    while (q.next()) out.append(hydrate(q));
    return out;
}

bool SettingsRepository::removeProvider(qint64 id) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM ai_providers WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec()) {
        LOG_ERROR("SettingsRepository", "removeProvider: " + q.lastError().text());
        return false;
    }
    return true;
}

bool SettingsRepository::setDefaultProvider(qint64 id) {
    QSqlQuery q(m_db);
    if (!m_db.transaction()) return false;
    q.exec(QStringLiteral("UPDATE ai_providers SET is_default = 0"));
    q.prepare(QStringLiteral("UPDATE ai_providers SET is_default = 1, updated_at = ? WHERE id = ?"));
    q.addBindValue(nowUtc());
    q.addBindValue(id);
    bool ok = q.exec();
    ok = m_db.commit() && ok;
    return ok;
}

std::optional<dmc::ai::ProviderConfig>
SettingsRepository::resolveProviderConfig(qint64 id) const {
    auto s = providerById(id);
    if (!s) return std::nullopt;

    dmc::ai::ProviderConfig cfg;
    cfg.baseUrl         = s->baseUrl;
    cfg.chatModel       = s->chatModel;
    cfg.embeddingModel  = s->embeddingModel;
    cfg.requestTimeoutMs= s->requestTimeoutMs > 0 ? s->requestTimeoutMs : 60000;
    cfg.proxyUrl        = s->proxyUrl;

    // 从钥匙串注入 API Key
    if (m_creds && !s->apiKeyRef.isEmpty()) {
        auto key = m_creds->load(s->apiKeyRef);
        if (key) cfg.apiKey = *key;
    }
    return cfg;
}

void SettingsRepository::setValue(const QString& key, const QString& value) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO app_settings(key, value) VALUES (?, ?)"));
    q.addBindValue(key);
    q.addBindValue(value);
    q.exec();
}

QString SettingsRepository::value(const QString& key, const QString& defaultValue) const {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT value FROM app_settings WHERE key = ?"));
    q.addBindValue(key);
    if (!q.exec() || !q.next()) return defaultValue;
    return q.value(0).toString();
}

} // namespace dmc::storage
