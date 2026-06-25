// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — SettingsRepository
// AI 相关非敏感配置存储：Provider 配置（不含 API Key）、模型偏好、超时、代理。
//
// 注意边界：
//   - API Key 不在此存储，由 SecureCredentialStore 管理。
//   - 此处保存 apiKeyRef（凭据引用，如 "openai-default"），
//     SettingsRepository::loadProviderConfig() 内部用 SecureCredentialStore 把 key 注入。
//
// 存储：SQLite（与知识库同一数据库或独立 app.db），由 DbMigrator 创建表。
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include "ai/AIProvider.h"
#include "storage/SecureCredentialStore.h"

#include <QObject>
#include <QString>
#include <QList>
#include <QSqlDatabase>
#include <memory>
#include <optional>

namespace dmc::storage {

struct ProviderSettings {
    qint64  id = 0;         // SQLite 自增主键
    QString displayName;
    QString baseUrl;
    QString embeddingBaseUrl;
    QString apiKeyRef;      // SecureCredentialStore 中的 key
    QString chatModel;
    QString embeddingModel;
    int     requestTimeoutMs = 60000;
    QString proxyUrl;
    bool    isDefault = false;
};

class SettingsRepository : public QObject {
    Q_OBJECT
public:
    SettingsRepository(const QString& dbPath,
                       SecureCredentialStore* creds,
                       QObject* parent = nullptr);
    ~SettingsRepository() override;

    bool open(QString* error = nullptr);
    void close();

    // ─── Provider 配置 ────────────────────────────────────────────────────────
    qint64 upsertProvider(const ProviderSettings& s);
    std::optional<ProviderSettings> providerById(qint64 id) const;
    std::optional<ProviderSettings> defaultProvider() const;
    QList<ProviderSettings> allProviders() const;
    bool removeProvider(qint64 id);
    bool setDefaultProvider(qint64 id);

    // 把 ProviderSettings + 凭据 组装成 AIProvider 可直接 applyConfig 的结构
    // 凭据缺失时 apiKey 字段为空字符串
    std::optional<dmc::ai::ProviderConfig> resolveProviderConfig(qint64 id) const;

    // ─── 通用键值（非敏感的应用设置） ──────────────────────────────────────────
    void   setValue(const QString& key, const QString& value);
    QString value(const QString& key, const QString& defaultValue = {}) const;

private:
    QString                  m_dbPath;
    QSqlDatabase             m_db;
    SecureCredentialStore*  m_creds;
};

} // namespace dmc::storage
