// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — TabManager
// 多标签文档管理器
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <QWidget>
#include <QMap>
#include <QList>

class QTabWidget;

namespace dmc {

class DocumentSession;
class MarkdownEditor;

/// 标签页信息
struct TabInfo {
    DocumentSession* session = nullptr;
    MarkdownEditor*  editor  = nullptr;
    int              tabId   = -1;
};

class TabManager : public QWidget {
    Q_OBJECT

public:
    explicit TabManager(QWidget* parent = nullptr);
    ~TabManager() override;

    // ─── 标签操作 ─────────────────────────────────────────────────────────
    DocumentSession* addNewTab();
    DocumentSession* openFile(const QString& filePath);
    bool closeTab(int index, bool forceClose = false);
    bool closeCurrentTab(bool forceClose = false);
    void closeAllTabs(bool forceClose = false);
    void closeOtherTabs(int index);
    void closeTabsToTheRight(int index);

    // ─── 切换 ─────────────────────────────────────────────────────────────
    void switchToTab(int index);
    void switchToNextTab();
    void switchToPreviousTab();

    // ─── 查询 ─────────────────────────────────────────────────────────────
    int  tabCount() const;
    int  currentIndex() const;
    DocumentSession* currentSession() const;
    MarkdownEditor*  currentEditor() const;
    DocumentSession* sessionAt(int index) const;
    MarkdownEditor*  editorAt(int index) const;
    QList<TabInfo> allTabs() const { return m_tabs; }

    // ─── 保存状态 ─────────────────────────────────────────────────────────
    bool saveCurrentTab();
    bool saveCurrentTabAs();
    bool saveTab(int index);
    bool saveAllTabs();

    // ─── 标签重命名 ───────────────────────────────────────────────────────
    void updateTabTitle(int index);
    void updateAllTabTitles();

signals:
    void currentTabChanged(DocumentSession* session);
    void tabCountChanged(int count);
    void tabClosed(int index);
    void requestClose(DocumentSession* session);

private slots:
    void onCurrentChanged(int index);
    void onTabCloseRequested(int index);
    void onContentModified();
    void onSaveStatusChanged();

private:
    DocumentSession* createSession();
    MarkdownEditor*  createEditor(DocumentSession* session);
    int  findTabBySession(DocumentSession* session) const;
    bool confirmClose(DocumentSession* session);

    QTabWidget*       m_tabWidget = nullptr;
    QList<TabInfo>    m_tabs;
    int               m_nextTabId = 1;
};

} // namespace dmc
