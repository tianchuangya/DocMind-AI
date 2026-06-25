// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — LocalOnlySyncProvider
// 首期默认实现：不接入云端，所有方法返回 NotImplemented。
// 保留作为占位，让上层依赖 SyncProvider 抽象而非具体实现。
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include "sync/SyncProvider.h"

namespace dmc::sync {

class LocalOnlySyncProvider : public SyncProvider {
    Q_OBJECT
public:
    explicit LocalOnlySyncProvider(QObject* parent = nullptr)
        : SyncProvider(parent) {}

    bool    isConfigured() const override { return false; }
    QString endpoint() const override     { return {}; }

    PullResult pull(const QDateTime&) override {
        return PullResult{false, QStringLiteral("Sync not configured (local-only)."), {}, {}};
    }
    PushResult push(const QList<ChangeRecord>&) override {
        return PushResult{false, QStringLiteral("Sync not configured (local-only)."), {}, {}};
    }
    PushResult resolveConflicts(const QList<ConflictResolution>&) override {
        return PushResult{false, QStringLiteral("Sync not configured (local-only)."), {}, {}};
    }
};

} // namespace dmc::sync
