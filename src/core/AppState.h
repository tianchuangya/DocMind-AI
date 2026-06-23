// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — AppState
// 全局应用状态（单例）：设置、最近文件、主题等
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QFont>
#include <QColor>
#include <memory>

namespace dmc {

/// 预览模式
enum class ViewMode {
    EditorOnly   = 0,
    SplitView    = 1,
    PreviewOnly  = 2,
};

/// 主题模式
enum class ThemeMode {
    Light = 0,
    Dark  = 1,
};

// ─── 最近文件记录 ─────────────────────────────────────────────────────────────
struct RecentFileEntry {
    QString filePath;
    qint64  lastOpened = 0;   // Unix timestamp

    bool operator==(const RecentFileEntry& o) const { return filePath == o.filePath; }
};

// ─── 应用状态 ─────────────────────────────────────────────────────────────────
class AppState : public QObject {
    Q_OBJECT

public:
    static AppState& instance();

    /// 加载配置（从 JSON 文件）
    void loadSettings();
    /// 保存配置
    void saveSettings();

    // ─── 最近文件 ─────────────────────────────────────────────────────────
    void addRecentFile(const QString& filePath);
    void clearRecentFiles();
    QList<RecentFileEntry> recentFiles() const { return m_recentFiles; }
    int maxRecentFiles() const { return m_maxRecentFiles; }

    // ─── 编辑器设置 ───────────────────────────────────────────────────────
    QFont editorFont() const { return m_editorFont; }
    void  setEditorFont(const QFont& font);
    int   tabSize() const { return m_tabSize; }
    void  setTabSize(int size);
    bool  wordWrap() const { return m_wordWrap; }
    void  setWordWrap(bool enabled);
    bool  showLineNumbers() const { return m_showLineNumbers; }
    void  setShowLineNumbers(bool show);
    int   autoSaveInterval() const { return m_autoSaveInterval; }
    void  setAutoSaveInterval(int seconds);
    bool  autoSaveEnabled() const { return m_autoSaveEnabled; }
    void  setAutoSaveEnabled(bool enabled);

    // ─── 视图设置 ─────────────────────────────────────────────────────────
    ViewMode viewMode() const { return m_viewMode; }
    void     setViewMode(ViewMode mode);
    ThemeMode themeMode() const { return m_themeMode; }
    void     setThemeMode(ThemeMode mode);

    // ─── 路径 ─────────────────────────────────────────────────────────────
    QString configDir() const;
    QString logDir() const;
    QString autoSaveDir() const;
    QString configFile() const;

    // ─── 窗口状态 ─────────────────────────────────────────────────────────
    QByteArray windowGeometry() const { return m_windowGeometry; }
    void setWindowGeometry(const QByteArray& geo);
    QByteArray windowState() const { return m_windowState; }
    void setWindowState(const QByteArray& state);
    QStringList openFilesOnStartup() const { return m_openFilesOnStartup; }
    void setOpenFilesOnStartup(const QStringList& files) { m_openFilesOnStartup = files; }
    int lastActiveTab() const { return m_lastActiveTab; }
    void setLastActiveTab(int idx) { m_lastActiveTab = idx; }

signals:
    void recentFilesChanged();
    void editorFontChanged(const QFont& font);
    void tabSizeChanged(int size);
    void wordWrapChanged(bool enabled);
    void showLineNumbersChanged(bool show);
    void autoSaveSettingsChanged();
    void viewModeChanged(ViewMode mode);
    void themeModeChanged(ThemeMode mode);

private:
    AppState();
    ~AppState() override;
    AppState(const AppState&) = delete;
    AppState& operator=(const AppState&) = delete;

    void ensureConfigDir() const;

    // 最近文件
    QList<RecentFileEntry> m_recentFiles;
    int m_maxRecentFiles = 15;

    // 编辑器
    QFont m_editorFont;
    int   m_tabSize           = 4;
    bool  m_wordWrap          = true;
    bool  m_showLineNumbers   = true;
    int   m_autoSaveInterval  = 60;   // 秒
    bool  m_autoSaveEnabled   = true;

    // 视图
    ViewMode  m_viewMode  = ViewMode::SplitView;
    ThemeMode m_themeMode = ThemeMode::Light;

    // 窗口
    QByteArray  m_windowGeometry;
    QByteArray  m_windowState;
    QStringList m_openFilesOnStartup;
    int         m_lastActiveTab = 0;
};

} // namespace dmc
