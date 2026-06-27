// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — AppState 实现
// ─────────────────────────────────────────────────────────────────────────────
#include "core/AppState.h"
#include "utils/Logger.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFontDatabase>
#include <algorithm>
#include <QDateTime>

namespace dmc {

AppState& AppState::instance() {
    static AppState s_instance;
    return s_instance;
}

AppState::AppState() {
    // 默认等宽字体
    m_editorFont = QFont("Consolas", 12);
    m_editorFont.setStyleHint(QFont::Monospace);
}

AppState::~AppState() {
    saveSettings();
}

// ─── 路径 ────────────────────────────────────────────────────────────────────

QString AppState::configDir() const {
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + "/.docmind-ai";
    }
    QDir d(base);
    if (!d.exists()) d.mkpath(".");
    return base;
}

QString AppState::logDir() const {
    QString path = configDir() + "/logs";
    QDir d(path);
    if (!d.exists()) d.mkpath(".");
    return path;
}

QString AppState::autoSaveDir() const {
    QString path = configDir() + "/autosave";
    QDir d(path);
    if (!d.exists()) d.mkpath(".");
    return path;
}

QString AppState::configFile() const {
    return configDir() + "/settings.json";
}

void AppState::ensureConfigDir() const {
    QDir d(configDir());
    if (!d.exists()) d.mkpath(".");
}

// ─── 加载 / 保存 ─────────────────────────────────────────────────────────────

void AppState::loadSettings() {
    ensureConfigDir();

    QFile file(configFile());
    if (!file.exists()) {
        LOG_INFO("AppState", "配置文件不存在，使用默认设置");
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        LOG_WARN("AppState", "无法打开配置文件: " + file.errorString());
        return;
    }

    QJsonParseError parseError;
    auto doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError) {
        LOG_ERROR("AppState", "配置文件解析失败: " + parseError.errorString());
        return;
    }

    QJsonObject root = doc.object();

    // 编辑器
    QJsonObject editor = root["editor"].toObject();
    if (editor.contains("fontFamily")) {
        m_editorFont.setFamily(editor["fontFamily"].toString());
    }
    if (editor.contains("fontSize")) {
        m_editorFont.setPointSize(editor["fontSize"].toInt());
    }
    if (editor.contains("tabSize"))          m_tabSize         = editor["tabSize"].toInt();
    if (editor.contains("wordWrap"))         m_wordWrap        = editor["wordWrap"].toBool();
    if (editor.contains("showLineNumbers"))  m_showLineNumbers = editor["showLineNumbers"].toBool();
    if (editor.contains("autoSaveInterval")) m_autoSaveInterval= editor["autoSaveInterval"].toInt();
    if (editor.contains("autoSaveEnabled"))  m_autoSaveEnabled = editor["autoSaveEnabled"].toBool();

    // 视图
    QJsonObject view = root["view"].toObject();
    if (view.contains("viewMode"))  m_viewMode  = static_cast<ViewMode>(view["viewMode"].toInt());
    if (view.contains("themeMode")) m_themeMode = static_cast<ThemeMode>(view["themeMode"].toInt());

    // 窗口
    QJsonObject window = root["window"].toObject();
    if (window.contains("geometry")) {
        m_windowGeometry = QByteArray::fromBase64(
            window["geometry"].toString().toLatin1());
    }
    if (window.contains("state")) {
        m_windowState = QByteArray::fromBase64(
            window["state"].toString().toLatin1());
    }
    if (window.contains("openFiles")) {
        QJsonArray arr = window["openFiles"].toArray();
        m_openFilesOnStartup.clear();
        for (const auto& v : arr) {
            m_openFilesOnStartup.append(v.toString());
        }
    }
    if (window.contains("lastActiveTab")) {
        m_lastActiveTab = window["lastActiveTab"].toInt();
    }

    // 最近文件
    QJsonObject recent = root["recentFiles"].toObject();
    if (recent.contains("maxCount")) m_maxRecentFiles = recent["maxCount"].toInt();
    QJsonArray filesArr = recent["files"].toArray();
    m_recentFiles.clear();
    for (const auto& v : filesArr) {
        QJsonObject obj = v.toObject();
        RecentFileEntry entry;
        entry.filePath   = obj["path"].toString();
        entry.lastOpened = static_cast<qint64>(obj["lastOpened"].toDouble());
        if (QFile::exists(entry.filePath)) {
            m_recentFiles.append(entry);
        }
    }

    LOG_INFO("AppState", "配置加载完成");
}

void AppState::saveSettings() {
    ensureConfigDir();

    QJsonObject root;

    // 编辑器
    QJsonObject editor;
    editor["fontFamily"]      = m_editorFont.family();
    editor["fontSize"]        = m_editorFont.pointSize();
    editor["tabSize"]         = m_tabSize;
    editor["wordWrap"]        = m_wordWrap;
    editor["showLineNumbers"] = m_showLineNumbers;
    editor["autoSaveInterval"]= m_autoSaveInterval;
    editor["autoSaveEnabled"] = m_autoSaveEnabled;
    root["editor"] = editor;

    // 视图
    QJsonObject view;
    view["viewMode"]  = static_cast<int>(m_viewMode);
    view["themeMode"] = static_cast<int>(m_themeMode);
    root["view"] = view;

    // 窗口
    QJsonObject window;
    window["geometry"]     = QString::fromLatin1(m_windowGeometry.toBase64());
    window["state"]        = QString::fromLatin1(m_windowState.toBase64());
    window["lastActiveTab"]= m_lastActiveTab;
    QJsonArray openFilesArr;
    for (const auto& f : m_openFilesOnStartup) {
        openFilesArr.append(f);
    }
    window["openFiles"] = openFilesArr;
    root["window"] = window;

    // 最近文件
    QJsonObject recent;
    recent["maxCount"] = m_maxRecentFiles;
    QJsonArray filesArr;
    for (const auto& entry : m_recentFiles) {
        QJsonObject obj;
        obj["path"]       = entry.filePath;
        obj["lastOpened"] = static_cast<double>(entry.lastOpened);
        filesArr.append(obj);
    }
    recent["files"] = filesArr;
    root["recentFiles"] = recent;

    QFile file(configFile());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        LOG_ERROR("AppState", "无法保存配置文件: " + file.errorString());
        return;
    }

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    LOG_INFO("AppState", "配置已保存");
}

// ─── 最近文件 ─────────────────────────────────────────────────────────────────

void AppState::addRecentFile(const QString& filePath) {
    QString canonical = QDir::toNativeSeparators(filePath);

    // 移除已有记录
    m_recentFiles.erase(
        std::remove_if(m_recentFiles.begin(), m_recentFiles.end(),
            [&](const RecentFileEntry& e) { return e.filePath == canonical; }),
        m_recentFiles.end());

    // 插入到头部
    RecentFileEntry entry;
    entry.filePath   = canonical;
    entry.lastOpened = QDateTime::currentSecsSinceEpoch();
    m_recentFiles.prepend(entry);

    // 截断
    while (m_recentFiles.size() > m_maxRecentFiles) {
        m_recentFiles.removeLast();
    }

    emit recentFilesChanged();
}

void AppState::clearRecentFiles() {
    m_recentFiles.clear();
    emit recentFilesChanged();
}

// ─── 设置器（自动保存 + 发信号） ──────────────────────────────────────────────

void AppState::setEditorFont(const QFont& font) {
    if (m_editorFont == font) return;
    m_editorFont = font;
    emit editorFontChanged(font);
}

void AppState::setTabSize(int size) {
    if (m_tabSize == size) return;
    m_tabSize = size;
    emit tabSizeChanged(size);
}

void AppState::setWordWrap(bool enabled) {
    if (m_wordWrap == enabled) return;
    m_wordWrap = enabled;
    emit wordWrapChanged(enabled);
}

void AppState::setShowLineNumbers(bool show) {
    if (m_showLineNumbers == show) return;
    m_showLineNumbers = show;
    emit showLineNumbersChanged(show);
}

void AppState::setAutoSaveInterval(int seconds) {
    if (m_autoSaveInterval == seconds) return;
    m_autoSaveInterval = seconds;
    emit autoSaveSettingsChanged();
}

void AppState::setAutoSaveEnabled(bool enabled) {
    if (m_autoSaveEnabled == enabled) return;
    m_autoSaveEnabled = enabled;
    emit autoSaveSettingsChanged();
}

void AppState::setViewMode(ViewMode mode) {
    if (m_viewMode == mode) return;
    m_viewMode = mode;
    emit viewModeChanged(mode);
}

void AppState::setThemeMode(ThemeMode mode) {
    if (m_themeMode == mode) return;
    m_themeMode = mode;
    emit themeModeChanged(mode);
}

void AppState::setWindowGeometry(const QByteArray& geo) {
    m_windowGeometry = geo;
}

void AppState::setWindowState(const QByteArray& state) {
    m_windowState = state;
}

} // namespace dmc
