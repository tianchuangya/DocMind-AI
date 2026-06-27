// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — DocumentSession
// 文档会话：管理一个文档的完整生命周期（路径、内容、状态、快照）
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <QObject>
#include <QString>
#include <QTextCursor>
#include <QDateTime>
#include <QTemporaryFile>

namespace dmc {

/// 文档保存状态
enum class SaveStatus {
    Clean,       // 已保存，无修改
    Modified,    // 有未保存修改
    New,         // 新建未保存
    Saving,      // 正在保存
    Error,       // 保存失败
};

/// 文档编码
enum class FileEncoding {
    UTF8,
    UTF16,
    Latin1,
    System,
};

// ─── 文档快照（只读，用于传递给渲染器 / 导出器） ─────────────────────────────
struct DocumentSnapshot {
    QString   content;          // Markdown 源码
    QString   filePath;         // 文件路径（可能为空）
    QString   title;            // 文档标题
    QString   baseDir;          // 所在目录（用于解析相对路径图片）
    qint64    renderVersion = 0;// 每次修改递增，用于防抖比对
    bool      isModified    = false;
};

// ─── 文档会话 ────────────────────────────────────────────────────────────────
class DocumentSession : public QObject {
    Q_OBJECT

public:
    explicit DocumentSession(QObject* parent = nullptr);
    ~DocumentSession() override;

    // ─── 文件操作 ─────────────────────────────────────────────────────────
    bool openFile(const QString& filePath, QString* error = nullptr);
    bool saveFile(QString* error = nullptr);
    bool saveFileAs(const QString& filePath, QString* error = nullptr);
    void newDocument();

    // ─── 内容 ─────────────────────────────────────────────────────────────
    QString content() const;
    void    setContent(const QString& text);
    void    insertText(const QString& text);

    // ─── 状态 ─────────────────────────────────────────────────────────────
    QString     filePath()    const { return m_filePath; }
    QString     fileName()    const;
    QString     title()       const;
    QString     baseDir()     const;
    bool        isModified()  const { return m_isModified; }
    bool        isNewFile()   const { return m_filePath.isEmpty(); }
    SaveStatus  saveStatus()  const { return m_saveStatus; }
    qint64      renderVersion() const { return m_renderVersion; }
    QDateTime   lastModified()  const { return m_lastModified; }
    qint64      fileSize()      const { return m_fileSize; }

    // ─── 光标位置 ─────────────────────────────────────────────────────────
    int  cursorLine()   const { return m_cursorLine; }
    int  cursorColumn() const { return m_cursorColumn; }
    void setCursorPosition(int line, int col);

    // ─── 快照 ─────────────────────────────────────────────────────────────
    DocumentSnapshot snapshot() const;

    // ─── 编码 ─────────────────────────────────────────────────────────────
    FileEncoding encoding() const { return m_encoding; }

signals:
    void contentChanged();
    void saveStatusChanged(SaveStatus status);
    void cursorPositionChanged(int line, int col);
    void filePathChanged(const QString& path);
    void aboutToClose();

public slots:
    void setModified(bool modified);

private:
    void updateTitle();
    bool writeFile(const QString& filePath, const QString& content,
                   QString* error = nullptr);

    QString     m_filePath;
    QString     m_title;
    QString     m_content;
    bool        m_isModified   = false;
    SaveStatus  m_saveStatus   = SaveStatus::New;
    qint64      m_renderVersion = 0;
    QDateTime   m_lastModified;
    qint64      m_fileSize     = 0;
    FileEncoding m_encoding    = FileEncoding::UTF8;
    int         m_cursorLine   = 1;
    int         m_cursorColumn = 1;
};

} // namespace dmc
