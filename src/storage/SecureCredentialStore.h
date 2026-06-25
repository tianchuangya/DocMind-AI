// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — SecureCredentialStore
// 敏感凭据存储：系统钥匙串（macOS Keychain / Windows Credential Manager / Linux Secret Service）。
//
// 职责：仅保存 API Key 这类敏感数据；不写日志原文。
// 模块 A 的 AppState 不持有 API Key；模块 C 的 SettingsRepository 只持有引用 id。
//
// 首期实现：跨平台封装。macOS 用 SecKeychain，Windows 用 CredWrite/CredRead，
// Linux 用 libsecret（DBus），不可用则回退到加密文件（AES-256-GCM，密钥派生自机器 ID）。
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <QObject>
#include <QString>
#include <optional>

namespace dmc::storage {

class SecureCredentialStore : public QObject {
    Q_OBJECT
public:
    explicit SecureCredentialStore(QObject* parent = nullptr);
    ~SecureCredentialStore() override;

    // 是否可用：原生钥匙串存在时返回 true；回退到加密文件时也返回 true
    bool isAvailable() const;

    // 存：成功返回 true；服务名建议固定为应用名（DocMindAI）
    bool store(const QString& key, const QString& secret);
    // 取：不存在返回 std::nullopt
    std::optional<QString> load(const QString& key);
    // 删
    bool remove(const QString& key);

private:
    // 平台原生实现（子文件实现）
    bool nativeStore(const QString& key, const QString& secret);
    std::optional<QString> nativeLoad(const QString& key);
    bool nativeRemove(const QString& key);

    // 加密文件回退实现
    QString fallbackPath() const;
    bool fallbackStore(const QString& key, const QString& secret);
    std::optional<QString> fallbackLoad(const QString& key);
    bool fallbackRemove(const QString& key);

    bool m_nativeAvailable = false;
};

} // namespace dmc::storage
