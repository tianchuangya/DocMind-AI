// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — DocumentSession 实现
// ─────────────────────────────────────────────────────────────────────────────
#include "core/DocumentSession.h"
#include "utils/Logger.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QStringConverter>

namespace dmc {

DocumentSession::DocumentSession(QObject* parent)
    : QObject(parent)
{
    m_title = QStringLiteral("未命名文档");
    m_lastModified = QDateTime::currentDateTime();
}

DocumentSession::~DocumentSession() {
    emit aboutToClose();
}

// ─── 文件操作 ────────────────────────────────────────────────────────────────

bool DocumentSession::openFile(const QString& filePath, QString* error) {
    QFile file(filePath);
    if (!file.exists()) {
        if (error) *error = QStringLiteral("文件不存在: %1").arg(filePath);
        LOG_ERROR("DocumentSession", "文件不存在: " + filePath);
        return false;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("无法打开文件: %1").arg(file.errorString());
        LOG_ERROR("DocumentSession", "无法打开文件: " + file.errorString());
        return false;
    }

    // 检测 BOM 并决定编码
    QByteArray raw = file.readAll();
    file.close();

    QString text;
    if (raw.startsWith("\xEF\xBB\xBF")) {
        // UTF-8 BOM
        text = QString::fromUtf8(raw.mid(3));
        m_encoding = FileEncoding::UTF8;
    } else if (raw.startsWith("\xFF\xFE")) {
        // UTF-16 LE
        text = QString::fromUtf16(reinterpret_cast<const char16_t*>(raw.constData() + 2),
                                   (raw.size() - 2) / 2);
        m_encoding = FileEncoding::UTF16;
    } else if (raw.startsWith("\xFE\xFF")) {
        // UTF-16 BE
        QByteArray swapped(raw);
        for (int i = 0; i < swapped.size() - 1; i += 2) {
            std::swap(swapped[i], swapped[i + 1]);
        }
        text = QString::fromUtf16(reinterpret_cast<const char16_t*>(swapped.constData() + 2),
                                   (swapped.size() - 2) / 2);
        m_encoding = FileEncoding::UTF16;
    } else {
        // 尝试 UTF-8，失败回退系统编码
        auto decoder = QStringDecoder(QStringDecoder::Utf8);
        if (!decoder.isValid()) {
            decoder = QStringDecoder(QStringDecoder::System);
        }
        text = decoder(raw);
        m_encoding = FileEncoding::UTF8;
    }

    m_filePath     = QDir::toNativeSeparators(filePath);
    m_content      = text;
    m_fileSize     = raw.size();
    m_isModified   = false;
    m_saveStatus   = SaveStatus::Clean;
    m_lastModified = QFileInfo(filePath).lastModified();
    m_renderVersion = 0;

    updateTitle();

    emit filePathChanged(m_filePath);
    emit contentChanged();
    emit saveStatusChanged(m_saveStatus);

    LOG_INFO("DocumentSession", QString("已打开文件: %1 (%2 bytes)").arg(filePath).arg(raw.size()));
    return true;
}

bool DocumentSession::saveFile(QString* error) {
    if (m_filePath.isEmpty()) {
        if (error) *error = QStringLiteral("文件路径为空，请使用另存为");
        return false;
    }
    return writeFile(m_filePath, m_content, error);
}

bool DocumentSession::saveFileAs(const QString& filePath, QString* error) {
    if (writeFile(filePath, m_content, error)) {
        m_filePath = QDir::toNativeSeparators(filePath);
        updateTitle();
        emit filePathChanged(m_filePath);
        return true;
    }
    return false;
}

void DocumentSession::newDocument() {
    m_filePath.clear();
    m_content.clear();
    m_title = QStringLiteral("未命名文档");
    m_isModified   = false;
    m_saveStatus   = SaveStatus::New;
    m_renderVersion = 0;
    m_fileSize     = 0;
    m_lastModified = QDateTime::currentDateTime();

    emit filePathChanged(QString());
    emit contentChanged();
    emit saveStatusChanged(m_saveStatus);
}

// ─── 内容 ────────────────────────────────────────────────────────────────────

QString DocumentSession::content() const {
    return m_content;
}

void DocumentSession::setContent(const QString& text) {
    m_content = text;
    m_renderVersion++;
    m_lastModified = QDateTime::currentDateTime();

    if (!m_isModified) {
        setModified(true);
    }

    emit contentChanged();
}

void DocumentSession::insertText(const QString& text) {
    m_content += text;
    m_renderVersion++;
    m_lastModified = QDateTime::currentDateTime();

    if (!m_isModified) {
        setModified(true);
    }

    emit contentChanged();
}

// ─── 状态 ────────────────────────────────────────────────────────────────────

QString DocumentSession::fileName() const {
    if (m_filePath.isEmpty()) return m_title;
    return QFileInfo(m_filePath).fileName();
}

QString DocumentSession::title() const {
    return m_title;
}

QString DocumentSession::baseDir() const {
    if (m_filePath.isEmpty()) return QDir::currentPath();
    return QFileInfo(m_filePath).absolutePath();
}

void DocumentSession::setCursorPosition(int line, int col) {
    if (m_cursorLine == line && m_cursorColumn == col) return;
    m_cursorLine   = line;
    m_cursorColumn = col;
    emit cursorPositionChanged(line, col);
}

DocumentSnapshot DocumentSession::snapshot() const {
    DocumentSnapshot s;
    s.content       = m_content;
    s.filePath      = m_filePath;
    s.title         = m_title;
    s.baseDir       = baseDir();
    s.renderVersion = m_renderVersion;
    s.isModified    = m_isModified;
    return s;
}

// ─── 私有 ────────────────────────────────────────────────────────────────────

void DocumentSession::setModified(bool modified) {
    if (m_isModified == modified) return;
    m_isModified = modified;

    if (m_isModified) {
        m_saveStatus = SaveStatus::Modified;
    } else {
        m_saveStatus = m_filePath.isEmpty() ? SaveStatus::New : SaveStatus::Clean;
    }

    updateTitle();
    emit saveStatusChanged(m_saveStatus);
}

void DocumentSession::updateTitle() {
    if (m_filePath.isEmpty()) {
        m_title = QStringLiteral("未命名文档");
    } else {
        m_title = QFileInfo(m_filePath).fileName();
    }
}

bool DocumentSession::writeFile(const QString& filePath, const QString& content,
                                 QString* error) {
    m_saveStatus = SaveStatus::Saving;
    emit saveStatusChanged(m_saveStatus);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("无法写入文件: %1").arg(file.errorString());
        m_saveStatus = SaveStatus::Error;
        emit saveStatusChanged(m_saveStatus);
        LOG_ERROR("DocumentSession", "无法写入文件: " + file.errorString());
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << content;
    stream.flush();
    file.close();

    m_fileSize     = file.size();
    m_isModified   = false;
    m_saveStatus   = SaveStatus::Clean;
    m_lastModified = QFileInfo(filePath).lastModified();

    emit saveStatusChanged(m_saveStatus);
    LOG_INFO("DocumentSession", QString("已保存: %1 (%2 bytes)").arg(filePath).arg(m_fileSize));
    return true;
}

} // namespace dmc
