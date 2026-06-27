#include "ResourceManager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDateTime>

namespace dmc {
namespace conversion {

ResourceManager::ResourceManager(QObject* parent) : QObject(parent) {}

ResourceManager::~ResourceManager() {
    for (const QString& dir : m_created_dirs)
        cleanupTempDir(dir);
}

QString ResourceManager::createTempDir(const QString& prefix) {
    QString path = QDir::tempPath() + "/" + prefix +
                   QString::number(QDateTime::currentMSecsSinceEpoch());
    QDir().mkpath(path);
    m_created_dirs.append(path);
    return path;
}

void ResourceManager::cleanupTempDir(const QString& dir_path) {
    QDir dir(dir_path);
    if (dir.exists())
        dir.removeRecursively();
    m_created_dirs.removeAll(dir_path);
}

QSet<QString> ResourceManager::collectResourceReferences(const QString& markdown) const {
    QSet<QString> refs;

    // 图片引用: ![alt](path)
    QRegularExpression img_re("!\\[[^\\]]*\\]\\(([^)]+)\\)");
    auto it = img_re.globalMatch(markdown);
    while (it.hasNext()) {
        auto match = it.next();
        QString path = match.captured(1);
        if (!path.startsWith("http://") && !path.startsWith("https://") &&
            !path.startsWith("data:"))
            refs.insert(path);
    }

    // HTML img 标签
    QRegularExpression html_img_re("<img[^>]+src=[\"']([^\"']+)[\"']",
                                    QRegularExpression::CaseInsensitiveOption);
    it = html_img_re.globalMatch(markdown);
    while (it.hasNext()) {
        auto match = it.next();
        QString path = match.captured(1);
        if (!path.startsWith("http://") && !path.startsWith("https://") &&
            !path.startsWith("data:"))
            refs.insert(path);
    }

    // Markdown 链接中可能是本地文件的
    QRegularExpression link_re("\\[[^\\]]*\\]\\(([^)]+)\\)");
    it = link_re.globalMatch(markdown);
    while (it.hasNext()) {
        auto match = it.next();
        QString path = match.captured(1);
        if (!path.startsWith("http://") && !path.startsWith("https://") &&
            !path.startsWith("#") && !path.startsWith("mailto:") &&
            path.contains('.') && !path.contains(' '))
            refs.insert(path);
    }

    return refs;
}

bool ResourceManager::validateResources(const QSet<QString>& resources,
                                        const QString& base_dir) const {
    for (const QString& res : resources) {
        QString full_path = resolveRelativePath(base_dir, res);
        if (!QFileInfo(full_path).exists()) {
            emit const_cast<ResourceManager*>(this)->resourceNotFound(full_path);
            return false;
        }
    }
    return true;
}

bool ResourceManager::copyResources(const QSet<QString>& resources,
                                    const QString& source_dir,
                                    const QString& target_dir) {
    bool all_ok = true;
    QDir(target_dir).mkpath(".");

    for (const QString& res : resources) {
        QString src = resolveRelativePath(source_dir, res);
        QString dst = target_dir + "/" + QFileInfo(res).fileName();

        if (!QFileInfo(src).exists()) {
            emit resourceNotFound(src);
            all_ok = false;
            continue;
        }

        if (QFile::exists(dst)) QFile::remove(dst);
        if (!QFile::copy(src, dst)) {
            all_ok = false;
            continue;
        }
        emit resourceCopied(src, dst);
    }
    return all_ok;
}

QString ResourceManager::resolveRelativePath(const QString& base_dir,
                                             const QString& rel_path) const {
    if (rel_path.isEmpty()) return base_dir;
    if (QFileInfo(rel_path).isAbsolute()) return rel_path;
    return base_dir + "/" + rel_path;
}

bool ResourceManager::confirmOverwrite(const QString& path) const {
    return QFileInfo(path).exists();
}

} // namespace conversion
} // namespace dmc
