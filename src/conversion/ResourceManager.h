#pragma once

#include "Types.h"
#include <QObject>
#include <QDir>
#include <QSet>

namespace dmc {
namespace conversion {

/// 资源管理器 — 管理临时目录、资源文件复制/引用、清理
class ResourceManager : public QObject {
    Q_OBJECT
public:
    explicit ResourceManager(QObject* parent = nullptr);
    ~ResourceManager() override;

    void    setResourceRoot(const QString& p) { m_resource_root = p; }
    QString resourceRoot() const { return m_resource_root; }

    /// 为任务创建临时工作目录
    QString createTempDir(const QString& prefix = "dmc_conv_");

    /// 清理指定临时目录
    void cleanupTempDir(const QString& dir_path);

    /// 收集 Markdown 中的资源引用（图片、链接）
    QSet<QString> collectResourceReferences(const QString& markdown) const;

    /// 验证资源文件是否存在
    bool validateResources(const QSet<QString>& resources,
                           const QString& base_dir) const;

    /// 复制资源到目标目录
    bool copyResources(const QSet<QString>& resources,
                       const QString& source_dir,
                       const QString& target_dir);

    /// 解析相对路径
    QString resolveRelativePath(const QString& base_dir,
                                const QString& rel_path) const;

    /// 确认覆盖
    bool confirmOverwrite(const QString& path) const;

signals:
    void resourceNotFound(const QString& resource_path);
    void resourceCopied(const QString& source, const QString& target);

private:
    QString m_resource_root;
    QStringList m_created_dirs;  // 追踪已创建的临时目录
};

} // namespace conversion
} // namespace dmc
