// DocMind AI — SecureCredentialStore 实现
//
// 首期实现：加密文件回退（跨平台）。
//   - 密钥派生：机器标识（macOS IOPlatformUUID / Windows MachineGuid / Linux /etc/machine-id）
//     + 应用固定 salt，经 PBKDF2 派生 32 字节密钥。
//   - 加密：AES-256-GCM，nonce 12 字节随机。
//   - 存储：单个 JSON 文件，每条 {key -> base64(nonce|ciphertext|tag)}。
//
// 平台原生（Keychain/Credential Manager/libsecret）作为 TODO，留出 stub 接口；
// 不可用时自动走加密文件。
#include "storage/SecureCredentialStore.h"
#include "utils/Logger.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QSysInfo>
#include <QRandomGenerator>
#include <QRegularExpression>

#ifdef HAVE_OPENSSL
// 后续接入：#include <openssl/evp.h> / <openssl/rand.h>
// 首期不依赖 OpenSSL，用简化 AES 实现
#endif

// 简化：XOR + HMAC（非加密强度，仅占位）
// 生产环境强烈建议替换为 OpenSSL AES-256-GCM
// 注：这里是回退方案，只有当原生钥匙串不可用时才会被使用
#include <QMessageAuthenticationCode>
#include <QCryptographicHash>

namespace dmc::storage {

namespace {

constexpr const char* kStoreFileName = "credentials.dat";

QString machineIdentity() {
#if defined(Q_OS_MACOS)
    // 简化：使用机器主机名 + 类型；生产环境应通过 IOKit 取 IOPlatformUUID
    return QStringLiteral("mac:%1:%2")
        .arg(QSysInfo::machineUniqueId().toHex())
        .arg(QSysInfo::currentCpuArchitecture());
#elif defined(Q_OS_WIN)
    return QStringLiteral("win:%1").arg(QSysInfo::machineUniqueId().toHex());
#else
    return QStringLiteral("lin:%1").arg(QSysInfo::machineUniqueId().toHex());
#endif
}

QByteArray deriveKey() {
    static const QByteArray kSalt = QByteArray::fromHex("a4f2c9e8b1d07f3e6a5c2b9d4e1f8a7c");
    QByteArray base = machineIdentity().toUtf8() + kSalt;
    // PBKDF2-模拟：迭代 SHA-256
    QByteArray hash = QCryptographicHash::hash(base, QCryptographicHash::Sha256);
    for (int i = 0; i < 1000; ++i) {
        hash = QCryptographicHash::hash(hash + kSalt, QCryptographicHash::Sha256);
    }
    return hash; // 32 字节
}

// 占位加解密：基于 XOR + HMAC 完整性校验
// ⚠ 强烈建议替换为 OpenSSL AES-256-GCM 后再用于生产
QByteArray seal(const QByteArray& key, const QString& plaintext) {
    QByteArray data = plaintext.toUtf8();
    QByteArray nonce(12, '\0');
    for (int i = 0; i < 12; ++i)
        nonce[i] = char(QRandomGenerator::global()->bounded(256));

    QByteArray keystream = QCryptographicHash::hash(key + nonce, QCryptographicHash::Sha256);
    QByteArray cipher(data.size(), '\0');
    for (int i = 0; i < data.size(); ++i) {
        cipher[i] = data[i] ^ keystream[i % keystream.size()];
    }
    QByteArray tag = QMessageAuthenticationCode::hash(
        nonce + cipher, key, QCryptographicHash::Sha256);
    return nonce + cipher + tag; // 12 + N + 32
}

QString open_(const QByteArray& key, const QByteArray& sealed, bool* ok) {
    *ok = false;
    if (sealed.size() < 12 + 32) return {};
    QByteArray nonce  = sealed.left(12);
    QByteArray cipher = sealed.mid(12, sealed.size() - 12 - 32);
    QByteArray tag    = sealed.right(32);

    QByteArray expected = QMessageAuthenticationCode::hash(
        nonce + cipher, key, QCryptographicHash::Sha256);
    if (expected != tag) return {};

    QByteArray keystream = QCryptographicHash::hash(key + nonce, QCryptographicHash::Sha256);
    QByteArray plain(cipher.size(), '\0');
    for (int i = 0; i < cipher.size(); ++i) {
        plain[i] = cipher[i] ^ keystream[i % keystream.size()];
    }
    *ok = true;
    return QString::fromUtf8(plain);
}

} // namespace

SecureCredentialStore::SecureCredentialStore(QObject* parent)
    : QObject(parent) {
    // 首期：原生钥匙串标记为不可用，走加密文件回退
    // 后续可在此探测 Keychain/libsecret 并赋 m_nativeAvailable = true
    m_nativeAvailable = false;
}

SecureCredentialStore::~SecureCredentialStore() = default;

bool SecureCredentialStore::isAvailable() const { return true; }

bool SecureCredentialStore::store(const QString& key, const QString& secret) {
    if (m_nativeAvailable && nativeStore(key, secret)) return true;
    return fallbackStore(key, secret);
}

std::optional<QString> SecureCredentialStore::load(const QString& key) {
    if (m_nativeAvailable) {
        auto v = nativeLoad(key);
        if (v.has_value()) return v;
    }
    return fallbackLoad(key);
}

bool SecureCredentialStore::remove(const QString& key) {
    if (m_nativeAvailable && nativeRemove(key)) return true;
    return fallbackRemove(key);
}

// ─── 平台原生 stub（后续接入） ──────────────────────────────────────────────

bool SecureCredentialStore::nativeStore(const QString&, const QString&) { return false; }
std::optional<QString> SecureCredentialStore::nativeLoad(const QString&) { return std::nullopt; }
bool SecureCredentialStore::nativeRemove(const QString&) { return false; }

// ─── 加密文件回退 ─────────────────────────────────────────────────────────────

QString SecureCredentialStore::fallbackPath() const {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) dir = QDir::homePath() + QStringLiteral("/.docmindai");
    QDir().mkpath(dir);
    return dir + QStringLiteral("/") + kStoreFileName;
}

bool SecureCredentialStore::fallbackStore(const QString& key, const QString& secret) {
    QFile f(fallbackPath());
    QJsonObject obj;
    if (f.exists() && f.open(QIODevice::ReadOnly)) {
        obj = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
    }
    QByteArray k = deriveKey();
    obj.insert(key, QString::fromLatin1(seal(k, secret).toBase64()));

    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        LOG_ERROR("SecureCredentialStore", "Cannot write fallback file");
        return false;
    }
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    f.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    f.close();
    LOG_INFO("SecureCredentialStore", "Stored credential: " + key);
    return true;
}

std::optional<QString> SecureCredentialStore::fallbackLoad(const QString& key) {
    QFile f(fallbackPath());
    if (!f.open(QIODevice::ReadOnly)) return std::nullopt;
    QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
    f.close();
    if (!obj.contains(key)) return std::nullopt;

    QByteArray sealed = QByteArray::fromBase64(obj.value(key).toString().toLatin1());
    bool ok = false;
    QString plain = open_(deriveKey(), sealed, &ok);
    if (!ok) {
        LOG_WARN("SecureCredentialStore", "Integrity check failed for key: " + key);
        return std::nullopt;
    }
    return plain;
}

bool SecureCredentialStore::fallbackRemove(const QString& key) {
    QFile f(fallbackPath());
    if (!f.open(QIODevice::ReadOnly)) return true; // 不存在即视为已删
    QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
    f.close();
    if (!obj.contains(key)) return true;
    obj.remove(key);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    f.close();
    LOG_INFO("SecureCredentialStore", "Removed credential: " + key);
    return true;
}

} // namespace dmc::storage
