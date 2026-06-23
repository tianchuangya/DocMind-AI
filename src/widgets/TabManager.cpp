// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — TabManager 实现
// ─────────────────────────────────────────────────────────────────────────────
#include "widgets/TabManager.h"
#include "core/DocumentSession.h"
#include "editor/MarkdownEditor.h"
#include "core/AppState.h"
#include "utils/Logger.h"

#include <QTabWidget>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QTabBar>
#include <QSignalBlocker>

namespace dmc {

TabManager::TabManager(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_tabWidget = new QTabWidget();
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);
    m_tabWidget->setDocumentMode(true);
    m_tabWidget->setElideMode(Qt::ElideRight);
    m_tabWidget->setUsesScrollButtons(true);

    layout->addWidget(m_tabWidget);

    connect(m_tabWidget, &QTabWidget::currentChanged,
            this, &TabManager::onCurrentChanged);
    connect(m_tabWidget, &QTabWidget::tabCloseRequested,
            this, &TabManager::onTabCloseRequested);
}

TabManager::~TabManager() = default;

// ─── 标签操作 ────────────────────────────────────────────────────────────────

DocumentSession* TabManager::addNewTab() {
    DocumentSession* session = createSession();
    MarkdownEditor* editor = createEditor(session);

    TabInfo info;
    info.session = session;
    info.editor  = editor;
    info.tabId   = m_nextTabId++;

    // 先添加到 m_tabs，再 addTab（避免 currentChanged 信号触发时 m_tabs 为空）
    int index = m_tabs.size();
    m_tabs.append(info);

    {
        QSignalBlocker blocker(m_tabWidget);  // 阻止 addTab 期间触发 currentChanged
        m_tabWidget->addTab(editor, session->title());
    }

    m_tabWidget->setCurrentIndex(index);
    emit tabCountChanged(m_tabs.size());

    // 手动触发一次 currentTabChanged（因为 blocker 阻止了自动触发）
    emit currentTabChanged(session);

    LOG_INFO("TabManager", QString("新建标签页 #%1").arg(info.tabId));
    return session;
}

DocumentSession* TabManager::openFile(const QString& filePath) {
    // 检查文件是否已打开
    for (const TabInfo& info : m_tabs) {
        if (info.session->filePath() == QDir::toNativeSeparators(filePath)) {
            int idx = findTabBySession(info.session);
            if (idx >= 0) {
                m_tabWidget->setCurrentIndex(idx);
                return info.session;
            }
        }
    }

    DocumentSession* session = createSession();
    QString error;
    if (!session->openFile(filePath, &error)) {
        QMessageBox::warning(this, "打开失败", error);
        delete session;
        return nullptr;
    }

    MarkdownEditor* editor = createEditor(session);
    editor->setPlainText(session->content());

    TabInfo info;
    info.session = session;
    info.editor  = editor;
    info.tabId   = m_nextTabId++;

    // 先添加到 m_tabs，再 addTab
    int index = m_tabs.size();
    m_tabs.append(info);

    {
        QSignalBlocker blocker(m_tabWidget);
        m_tabWidget->addTab(editor, session->title());
    }

    m_tabWidget->setCurrentIndex(index);
    emit tabCountChanged(m_tabs.size());

    // 手动触发
    emit currentTabChanged(session);

    AppState::instance().addRecentFile(filePath);
    LOG_INFO("TabManager", "已打开: " + filePath);
    return session;
}

bool TabManager::closeTab(int index, bool forceClose) {
    if (index < 0 || index >= m_tabs.size()) return false;

    TabInfo& info = m_tabs[index];

    if (!forceClose && !confirmClose(info.session)) {
        return false;
    }

    m_tabWidget->removeTab(index);
    emit tabClosed(index);

    // 清理
    delete info.editor;
    delete info.session;
    m_tabs.removeAt(index);

    emit tabCountChanged(m_tabs.size());

    // 如果所有标签关闭，自动新建一个
    if (m_tabs.isEmpty()) {
        addNewTab();
    }

    return true;
}

bool TabManager::closeCurrentTab(bool forceClose) {
    return closeTab(m_tabWidget->currentIndex(), forceClose);
}

void TabManager::closeAllTabs(bool forceClose) {
    for (int i = m_tabs.size() - 1; i >= 0; i--) {
        if (!closeTab(i, forceClose)) break;
    }
}

void TabManager::closeOtherTabs(int index) {
    for (int i = m_tabs.size() - 1; i >= 0; i--) {
        if (i != index) closeTab(i);
    }
}

void TabManager::closeTabsToTheRight(int index) {
    for (int i = m_tabs.size() - 1; i > index; i--) {
        closeTab(i);
    }
}

// ─── 切换 ────────────────────────────────────────────────────────────────────

void TabManager::switchToTab(int index) {
    if (index >= 0 && index < m_tabs.size()) {
        m_tabWidget->setCurrentIndex(index);
    }
}

void TabManager::switchToNextTab() {
    int next = (m_tabWidget->currentIndex() + 1) % m_tabs.size();
    m_tabWidget->setCurrentIndex(next);
}

void TabManager::switchToPreviousTab() {
    int prev = m_tabWidget->currentIndex() - 1;
    if (prev < 0) prev = m_tabs.size() - 1;
    m_tabWidget->setCurrentIndex(prev);
}

// ─── 查询 ────────────────────────────────────────────────────────────────────

int TabManager::tabCount() const {
    return m_tabs.size();
}

int TabManager::currentIndex() const {
    return m_tabWidget->currentIndex();
}

DocumentSession* TabManager::currentSession() const {
    int idx = m_tabWidget->currentIndex();
    if (idx < 0 || idx >= m_tabs.size()) return nullptr;
    return m_tabs[idx].session;
}

MarkdownEditor* TabManager::currentEditor() const {
    int idx = m_tabWidget->currentIndex();
    if (idx < 0 || idx >= m_tabs.size()) return nullptr;
    return m_tabs[idx].editor;
}

DocumentSession* TabManager::sessionAt(int index) const {
    if (index < 0 || index >= m_tabs.size()) return nullptr;
    return m_tabs[index].session;
}

MarkdownEditor* TabManager::editorAt(int index) const {
    if (index < 0 || index >= m_tabs.size()) return nullptr;
    return m_tabs[index].editor;
}

// ─── 保存 ────────────────────────────────────────────────────────────────────

bool TabManager::saveCurrentTab() {
    return saveTab(m_tabWidget->currentIndex());
}

bool TabManager::saveCurrentTabAs() {
    DocumentSession* session = currentSession();
    if (!session) return false;

    QString filePath = QFileDialog::getSaveFileName(
        this, "另存为", session->baseDir(),
        "Markdown 文件 (*.md *.markdown);;所有文件 (*)");

    if (filePath.isEmpty()) return false;

    QString error;
    if (session->saveFileAs(filePath, &error)) {
        updateTabTitle(findTabBySession(session));
        return true;
    }

    QMessageBox::warning(this, "保存失败", error);
    return false;
}

bool TabManager::saveTab(int index) {
    if (index < 0 || index >= m_tabs.size()) return false;

    DocumentSession* session = m_tabs[index].session;
    if (!session->isModified()) return true;

    if (session->isNewFile()) {
        QString filePath = QFileDialog::getSaveFileName(
            this, "保存文件", session->baseDir(),
            "Markdown 文件 (*.md *.markdown);;所有文件 (*)");

        if (filePath.isEmpty()) return false;

        QString error;
        if (!session->saveFileAs(filePath, &error)) {
            QMessageBox::warning(this, "保存失败", error);
            return false;
        }
    } else {
        QString error;
        if (!session->saveFile(&error)) {
            QMessageBox::warning(this, "保存失败", error);
            return false;
        }
    }

    updateTabTitle(index);
    AppState::instance().addRecentFile(session->filePath());
    return true;
}

bool TabManager::saveAllTabs() {
    bool allOk = true;
    for (int i = 0; i < m_tabs.size(); i++) {
        if (!saveTab(i)) allOk = false;
    }
    return allOk;
}

// ─── 标签标题 ────────────────────────────────────────────────────────────────

void TabManager::updateTabTitle(int index) {
    if (index < 0 || index >= m_tabs.size()) return;

    DocumentSession* session = m_tabs[index].session;
    QString title = session->fileName();
    if (session->isModified()) {
        title += " ●";
    }
    m_tabWidget->setTabText(index, title);
    m_tabWidget->setTabToolTip(index, session->filePath());
}

void TabManager::updateAllTabTitles() {
    for (int i = 0; i < m_tabs.size(); i++) {
        updateTabTitle(i);
    }
}

// ─── 私有方法 ────────────────────────────────────────────────────────────────

DocumentSession* TabManager::createSession() {
    auto* session = new DocumentSession(this);

    connect(session, &DocumentSession::contentChanged,
            this, &TabManager::onContentModified);
    connect(session, &DocumentSession::saveStatusChanged,
            this, &TabManager::onSaveStatusChanged);

    return session;
}

MarkdownEditor* TabManager::createEditor(DocumentSession* session) {
    auto* editor = new MarkdownEditor();

    // 应用全局配置
    editor->setFont(AppState::instance().editorFont());
    editor->setTabSize(AppState::instance().tabSize());
    editor->setShowLineNumbers(AppState::instance().showLineNumbers());
    editor->setWordWrap(AppState::instance().wordWrap());

    // 将 session 的内容同步到编辑器
    connect(editor, &QPlainTextEdit::textChanged, this, [session, editor]() {
        QString text = editor->toPlainText();
        if (text != session->content()) {
            session->setContent(text);
        }
    });

    // 初始内容
    editor->setPlainText(session->content());

    return editor;
}

int TabManager::findTabBySession(DocumentSession* session) const {
    for (int i = 0; i < m_tabs.size(); i++) {
        if (m_tabs[i].session == session) return i;
    }
    return -1;
}

bool TabManager::confirmClose(DocumentSession* session) {
    if (!session->isModified()) return true;

    QString fileName = session->fileName();
    auto result = QMessageBox::question(
        this, "保存更改",
        QString("文件 \"%1\" 已修改，是否保存？").arg(fileName),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);

    if (result == QMessageBox::Cancel) return false;
    if (result == QMessageBox::Save) {
        return saveTab(findTabBySession(session));
    }
    return true;
}

// ─── 槽函数 ──────────────────────────────────────────────────────────────────

void TabManager::onCurrentChanged(int index) {
    if (index >= 0 && index < m_tabs.size()) {
        emit currentTabChanged(m_tabs[index].session);
    }
}

void TabManager::onTabCloseRequested(int index) {
    closeTab(index);
}

void TabManager::onContentModified() {
    auto* session = qobject_cast<DocumentSession*>(sender());
    if (!session) return;
    int idx = findTabBySession(session);
    if (idx >= 0) {
        updateTabTitle(idx);
    }
}

void TabManager::onSaveStatusChanged() {
    auto* session = qobject_cast<DocumentSession*>(sender());
    if (!session) return;
    int idx = findTabBySession(session);
    if (idx >= 0) {
        updateTabTitle(idx);
    }
}

} // namespace dmc
