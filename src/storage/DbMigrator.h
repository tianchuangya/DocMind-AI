// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — DbMigrator
// SQLite schema 迁移：版本化、幂等。
//
// 数据库列表（首期合并到单库 app.db，按表前缀区分模块）：
//   schema_version(version, applied_at)
//   ai_providers(...)                 -- SettingsRepository
//   app_settings(key, value)          -- SettingsRepository
//   documents(...)                     -- KnowledgeRepository
//   chunks(...)
//   chunk_fts(...)                     -- FTS5 虚拟表
//   embeddings(...)
//   kb_settings(...)                   -- 可选，知识库本地设置
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <QObject>
#include <QString>
#include <QSqlDatabase>

namespace dmc::storage {

class DbMigrator : public QObject {
    Q_OBJECT
public:
    explicit DbMigrator(const QSqlDatabase& db, QObject* parent = nullptr);

    // 当前 schema 版本；0 表示未初始化
    int currentVersion() const;

    // 执行所有待应用的迁移；返回是否全部成功
    bool migrate(QString* error = nullptr);

    // 最新 schema 版本常量
    static int latestVersion();

private:
    // 每个版本号对应一个迁移函数；从 currentVersion+1 应用到 latestVersion
    bool applyV1(QString* error);
    bool applyV2(QString* error);   // 预留：例如增加 ANN 索引表

    QSqlDatabase m_db;
};

} // namespace dmc::storage
