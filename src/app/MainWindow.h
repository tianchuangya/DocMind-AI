// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — MainWindow
// 主窗口：菜单栏、工具栏、状态栏、编辑器/预览布局
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <QMainWindow>
#include <QComboBox>
#include "core/AppState.h"

class QSplitter;
class QLabel;
class QTimer;

namespace dmc {

class TabManager;
class PreviewWidget;
class DocumentSession;
class MarkdownEditor;
class FindReplaceDialog;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    /// 打开文件
    void openFile(const QString& filePath);

    /// 获取标签管理器
    TabManager* tabManager() const { return m_tabManager; }

    /// 获取预览控件
    PreviewWidget* previewWidget() const { return m_preview; }

signals:
    void windowStateChanged();

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    // 文件操作
    void onNewFile();
    void onOpenFile();
    void onOpenRecentFile();
    void onSave();
    void onSaveAs();
    void onSaveAll();
    void onCloseTab();
    void onCloseAllTabs();
    void onCloseOtherTabs();

    // 编辑操作
    void onUndo();
    void onRedo();
    void onCut();
    void onCopy();
    void onPaste();
    void onSelectAll();
    void onFind();
    void onFindReplace();
    void onGoToLine();

    // Markdown 操作
    void onInsertHeading();
    void onInsertBold();
    void onInsertItalic();
    void onInsertCode();
    void onInsertCodeBlock();
    void onInsertLink();
    void onInsertImage();
    void onInsertTable();
    void onInsertTaskList();
    void onInsertBlockquote();
    void onInsertHorizontalRule();
    void onInsertOrderedList();
    void onInsertUnorderedList();

    // 视图操作
    void onToggleEditorOnly();
    void onToggleSplitView();
    void onTogglePreviewOnly();
    void onToggleTheme();
    void onToggleLineNumbers();
    void onToggleWordWrap();
    void onZoomIn();
    void onZoomOut();
    void onResetZoom();

    // 工具
    void onSettings();
    void onAbout();

    // 内部
    void onCurrentTabChanged(DocumentSession* session);
    void onCursorInfoChanged(int line, int col, int selLen);
    void onEditorContentModified();
    void onAutoSaveTimeout();
    void updateStatusBar();
    void updatePreview();

private:
    void setupUI();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupConnections();
    void setupAutoSave();
    void applyViewMode(ViewMode mode);

    // ─── 组件 ─────────────────────────────────────────────────────────────
    QSplitter*      m_splitter     = nullptr;
    TabManager*     m_tabManager   = nullptr;
    PreviewWidget*  m_preview      = nullptr;

    // ─── 状态栏 ───────────────────────────────────────────────────────────
    QLabel* m_cursorLabel   = nullptr;
    QLabel* m_lineLabel     = nullptr;
    QLabel* m_encodingLabel = nullptr;
    QLabel* m_modifiedLabel = nullptr;
    QComboBox* m_syncModeCombo = nullptr;

    // ─── 自动保存 ─────────────────────────────────────────────────────────
    QTimer* m_autoSaveTimer = nullptr;

    // ─── 对话框 ───────────────────────────────────────────────────────────
    FindReplaceDialog* m_findDialog = nullptr;

    // ─── 菜单 ─────────────────────────────────────────────────────────────
    QMenu* m_recentMenu = nullptr;
    QMenu* m_viewMenu   = nullptr;

    // ─── 状态 ─────────────────────────────────────────────────────────────
    ViewMode m_currentViewMode = ViewMode::SplitView;
};

} // namespace dmc
