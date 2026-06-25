// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — SyncProvider
// 同步扩展接口（首期仅数据契约，不接入云端服务）。
//
// 设计目标：保留未来云同步能力的扩展点；当前实现可返回"未实现"错误。
// 实现方需要支持：变更记录、拉取、推送、冲突检测。
//
// 数据所有权：本地 SQLite 是权威源；同步只读取并产生增量记录。
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QDateTime>
#include <QVariant>
#include <optional>

namespace dmc::sync {

enum class EntityType {
    KnowledgeDocument,
    Chunk,
    Embedding,
    ProviderSettings,
    AppSettings,
};

enum class ChangeKind {
    Created,
    Updated,
    Deleted,
};

struct ChangeRecord {
    qint64     localId   = 0;
    EntityType entityType = EntityType::Chunk;
    ChangeKind kind       = ChangeKind::Updated;
    QDateTime  occurredAt;
    QByteArray payloadHash;   // 内容哈希，用于冲突检测
    QVariant   payload;       // 实体序列化（JSON 或 row snapshot）
};

struct PullResult {
    bool        ok = false;
    QString     errorMessage;
    QList<ChangeRecord> remoteChanges;
    QString     serverVersion;   // 服务端版本标识
};

struct PushResult {
    bool        ok = false;
    QString     errorMessage;
    QList<qint64> acceptedLocalIds;   // 已被服务端接收
    QList<ChangeRecord> conflicts;     // 冲突条目（同 ID 不同哈希）
};

struct ConflictResolution {
    qint64 localId = 0;
    // LocalWins / RemoteWins / Merged（合并 payload）
    QString strategy;
    QVariant mergedPayload;
};

class SyncProvider : public QObject {
    Q_OBJECT
public:
    explicit SyncProvider(QObject* parent = nullptr) : QObject(parent) {}
    ~SyncProvider() override = default;

    // ─── 能力声明 ─────────────────────────────────────────────────────────────
    virtual bool   isConfigured() const = 0;      // 是否已配置远端
    virtual QString endpoint() const = 0;         // 远端 URL，未配置返回空

    // ─── 同步操作（首期可返回 NotImplemented） ─────────────────────────────────
    virtual PullResult pull(const QDateTime& since) = 0;
    virtual PushResult push(const QList<ChangeRecord>& localChanges) = 0;
    virtual PushResult resolveConflicts(const QList<ConflictResolution>& resolutions) = 0;

    // ─── 本地变更记录（由 Repository 在写入时通过 signal 通知） ───────────────
signals:
    void localChangeDetected(const ChangeRecord& record);
};

} // namespace dmc::sync
