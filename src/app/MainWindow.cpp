// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — MainWindow 实现
// ─────────────────────────────────────────────────────────────────────────────
#include "app/MainWindow.h"
#include "app/SettingsDialog.h"
#include "widgets/TabManager.h"
#include "widgets/ThemeManager.h"
#include "preview/PreviewWidget.h"
#include "preview/ScrollSyncManager.h"
#include "preview/MarkdownRenderer.h"
#include "editor/MarkdownEditor.h"
#include "editor/FindReplaceDialog.h"
#include "core/DocumentSession.h"
#include "core/AppState.h"
#include "utils/Logger.h"

#include <QSplitter>
#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QTimer>
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QInputDialog>
#include <QActionGroup>
#include <QApplication>
#include <QScreen>
#include <QVBoxLayout>

namespace dmc {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("DocMind AI - Markdown 编辑器");
    setMinimumSize(800, 600);
    setAcceptDrops(true);

    // 居中显示
    if (QScreen* screen = QApplication::primaryScreen()) {
        QRect screenGeo = screen->availableGeometry();
        int w = screenGeo.width() * 0.75;
        int h = screenGeo.height() * 0.8;
        setGeometry(
            screenGeo.center().x() - w / 2,
            screenGeo.center().y() - h / 2,
            w, h);
    }

    setupUI();
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    setupConnections();
    setupAutoSave();

    LOG_INFO("MainWindow", "主窗口已创建");
}

MainWindow::~MainWindow() {
    delete m_findDialog;
}

// ─── UI 搭建 ─────────────────────────────────────────────────────────────────

void MainWindow::setupUI() {
    // 中心部件
    auto* centralWidget = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 分栏器
    m_splitter = new QSplitter(Qt::Horizontal);

    // 标签管理器（编辑器）
    m_tabManager = new TabManager();

    // 预览区
    m_preview = new PreviewWidget();

    m_splitter->addWidget(m_tabManager);
    m_splitter->addWidget(m_preview);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(m_splitter);
    setCentralWidget(centralWidget);

    // 应用视图模式
    auto& state = AppState::instance();
    applyViewMode(state.viewMode());

    // 应用主题
    auto& theme = ThemeManager::instance();
    m_preview->setThemeCSS(theme.previewCSS());

    auto* editor = m_tabManager->currentEditor();
    if (editor) {
        if (theme.currentTheme() == ThemeMode::Dark) {
            editor->applyDarkTheme();
        } else {
            editor->applyLightTheme();
        }
    }
}

void MainWindow::setupMenuBar() {
    auto* menuBar = this->menuBar();

    // ─── 文件 ───────────────────────────────────────────────────────────
    auto* fileMenu = menuBar->addMenu("文件(&F)");

    auto* newAct = fileMenu->addAction("新建(&N)");
    newAct->setShortcut(QKeySequence::New);
    connect(newAct, &QAction::triggered, this, &MainWindow::onNewFile);

    auto* openAct = fileMenu->addAction("打开(&O)...");
    openAct->setShortcut(QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, &MainWindow::onOpenFile);

    m_recentMenu = fileMenu->addMenu("最近打开");
    connect(m_recentMenu, &QMenu::aboutToShow, this, [this]() {
        m_recentMenu->clear();
        auto files = AppState::instance().recentFiles();
        if (files.isEmpty()) {
            m_recentMenu->addAction("(无)")->setEnabled(false);
            return;
        }
        for (const auto& entry : files) {
            QAction* act = m_recentMenu->addAction(entry.filePath);
            act->setData(entry.filePath);
            connect(act, &QAction::triggered, this, &MainWindow::onOpenRecentFile);
        }
        m_recentMenu->addSeparator();
        m_recentMenu->addAction("清除最近文件", &AppState::instance(),
                                &AppState::clearRecentFiles);
    });

    fileMenu->addSeparator();
    auto* saveAct = fileMenu->addAction("保存(&S)");
    saveAct->setShortcut(QKeySequence::Save);
    connect(saveAct, &QAction::triggered, this, &MainWindow::onSave);

    auto* saveAsAct = fileMenu->addAction("另存为(&A)...");
    saveAsAct->setShortcut(QKeySequence("Ctrl+Shift+S"));
    connect(saveAsAct, &QAction::triggered, this, &MainWindow::onSaveAs);
    auto* saveAllAct = fileMenu->addAction("全部保存");
    connect(saveAllAct, &QAction::triggered, this, &MainWindow::onSaveAll);

    fileMenu->addSeparator();
    auto* closeAct = fileMenu->addAction("关闭标签");
    closeAct->setShortcut(QKeySequence("Ctrl+W"));
    connect(closeAct, &QAction::triggered, this, &MainWindow::onCloseTab);

    auto* closeAllAct = fileMenu->addAction("关闭所有标签");
    closeAllAct->setShortcut(QKeySequence("Ctrl+Shift+W"));
    connect(closeAllAct, &QAction::triggered, this, &MainWindow::onCloseAllTabs);

    fileMenu->addSeparator();
    auto* quitAct = fileMenu->addAction("退出(&Q)");
    quitAct->setShortcut(QKeySequence::Quit);
    connect(quitAct, &QAction::triggered, qApp, &QApplication::quit);

    // ─── 编辑 ───────────────────────────────────────────────────────────
    auto* editMenu = menuBar->addMenu("编辑(&E)");

    auto* undoAct = editMenu->addAction("撤销(&U)");
    undoAct->setShortcut(QKeySequence::Undo);
    connect(undoAct, &QAction::triggered, this, &MainWindow::onUndo);

    auto* redoAct = editMenu->addAction("重做(&R)");
    redoAct->setShortcut(QKeySequence::Redo);
    connect(redoAct, &QAction::triggered, this, &MainWindow::onRedo);
    editMenu->addSeparator();
    auto* cutAct = editMenu->addAction("剪切(&T)");
    cutAct->setShortcut(QKeySequence::Cut);
    connect(cutAct, &QAction::triggered, this, &MainWindow::onCut);

    auto* copyAct = editMenu->addAction("复制(&C)");
    copyAct->setShortcut(QKeySequence::Copy);
    connect(copyAct, &QAction::triggered, this, &MainWindow::onCopy);

    auto* pasteAct = editMenu->addAction("粘贴(&P)");
    pasteAct->setShortcut(QKeySequence::Paste);
    connect(pasteAct, &QAction::triggered, this, &MainWindow::onPaste);

    auto* selectAllAct = editMenu->addAction("全选(&A)");
    selectAllAct->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAct, &QAction::triggered, this, &MainWindow::onSelectAll);

    editMenu->addSeparator();
    auto* findAct = editMenu->addAction("查找(&F)");
    findAct->setShortcut(QKeySequence::Find);
    connect(findAct, &QAction::triggered, this, &MainWindow::onFind);

    auto* findReplaceAct = editMenu->addAction("查找和替换(&H)...");
    findReplaceAct->setShortcut(QKeySequence::Replace);
    connect(findReplaceAct, &QAction::triggered, this, &MainWindow::onFindReplace);

    auto* goToLineAct = editMenu->addAction("跳转到行(&G)...");
    goToLineAct->setShortcut(QKeySequence("Ctrl+G"));
    connect(goToLineAct, &QAction::triggered, this, &MainWindow::onGoToLine);

    // ─── Markdown ───────────────────────────────────────────────────────
    auto* mdMenu = menuBar->addMenu("Markdown(&M)");

    auto* headingMenu = mdMenu->addMenu("插入标题");
    for (int i = 1; i <= 6; i++) {
        QString text = QString("H%1").arg(i);
        headingMenu->addAction(text, this, [this, i]() {
            auto* editor = m_tabManager->currentEditor();
            if (editor) editor->insertHeading(i);
        });
    }

    mdMenu->addSeparator();
    auto* boldAct = mdMenu->addAction("粗体");
    boldAct->setShortcut(QKeySequence("Ctrl+B"));
    connect(boldAct, &QAction::triggered, this, &MainWindow::onInsertBold);

    auto* italicAct = mdMenu->addAction("斜体");
    italicAct->setShortcut(QKeySequence("Ctrl+I"));
    connect(italicAct, &QAction::triggered, this, &MainWindow::onInsertItalic);
    auto* strikeAct = mdMenu->addAction("删除线");
    strikeAct->setShortcut(QKeySequence("Ctrl+Shift+X"));
    connect(strikeAct, &QAction::triggered, this, [this]() {
        auto* editor = m_tabManager->currentEditor();
        if (editor) editor->insertStrikethrough();
    });
    auto* codeAct = mdMenu->addAction("行内代码");
    codeAct->setShortcut(QKeySequence("Ctrl+`"));
    connect(codeAct, &QAction::triggered, this, &MainWindow::onInsertCode);

    auto* codeBlockAct = mdMenu->addAction("代码块");
    codeBlockAct->setShortcut(QKeySequence("Ctrl+Shift+K"));
    connect(codeBlockAct, &QAction::triggered, this, &MainWindow::onInsertCodeBlock);

    mdMenu->addSeparator();
    auto* linkAct = mdMenu->addAction("插入链接");
    linkAct->setShortcut(QKeySequence("Ctrl+K"));
    connect(linkAct, &QAction::triggered, this, &MainWindow::onInsertLink);
    auto* imageAct = mdMenu->addAction("插入图片");
    connect(imageAct, &QAction::triggered, this, &MainWindow::onInsertImage);

    auto* tableAct = mdMenu->addAction("插入表格");
    connect(tableAct, &QAction::triggered, this, &MainWindow::onInsertTable);

    auto* taskListAct = mdMenu->addAction("任务列表");
    connect(taskListAct, &QAction::triggered, this, &MainWindow::onInsertTaskList);

    auto* hrAct = mdMenu->addAction("水平线");
    connect(hrAct, &QAction::triggered, this, &MainWindow::onInsertHorizontalRule);

    auto* orderedListAct = mdMenu->addAction("有序列表");
    connect(orderedListAct, &QAction::triggered, this, &MainWindow::onInsertOrderedList);

    auto* unorderedListAct = mdMenu->addAction("无序列表");
    connect(unorderedListAct, &QAction::triggered, this, &MainWindow::onInsertUnorderedList);

    // ─── 视图 ───────────────────────────────────────────────────────────
    m_viewMenu = menuBar->addMenu("视图(&V)");

    auto* viewGroup = new QActionGroup(this);

    auto* editorOnlyAct = m_viewMenu->addAction("仅编辑器");
    editorOnlyAct->setCheckable(true);
    editorOnlyAct->setActionGroup(viewGroup);
    connect(editorOnlyAct, &QAction::triggered, this, &MainWindow::onToggleEditorOnly);

    auto* splitAct = m_viewMenu->addAction("分栏视图");
    splitAct->setCheckable(true);
    splitAct->setChecked(true);
    splitAct->setActionGroup(viewGroup);
    connect(splitAct, &QAction::triggered, this, &MainWindow::onToggleSplitView);

    auto* previewOnlyAct = m_viewMenu->addAction("仅预览");
    previewOnlyAct->setCheckable(true);
    previewOnlyAct->setActionGroup(viewGroup);
    connect(previewOnlyAct, &QAction::triggered, this, &MainWindow::onTogglePreviewOnly);

    m_viewMenu->addSeparator();
    auto* themeAct = m_viewMenu->addAction("切换主题");
    themeAct->setShortcut(QKeySequence("Ctrl+Shift+T"));
    connect(themeAct, &QAction::triggered, this, &MainWindow::onToggleTheme);

    auto* lineNumbersAct = m_viewMenu->addAction("显示/隐藏行号");
    lineNumbersAct->setShortcut(QKeySequence("Ctrl+Shift+L"));
    connect(lineNumbersAct, &QAction::triggered, this, &MainWindow::onToggleLineNumbers);
    auto* wordWrapAct = m_viewMenu->addAction("自动换行");
    connect(wordWrapAct, &QAction::triggered, this, &MainWindow::onToggleWordWrap);

    m_viewMenu->addSeparator();
    auto* zoomInAct = m_viewMenu->addAction("放大");
    zoomInAct->setShortcut(QKeySequence::ZoomIn);
    connect(zoomInAct, &QAction::triggered, this, &MainWindow::onZoomIn);

    auto* zoomOutAct = m_viewMenu->addAction("缩小");
    zoomOutAct->setShortcut(QKeySequence::ZoomOut);
    connect(zoomOutAct, &QAction::triggered, this, &MainWindow::onZoomOut);

    auto* resetZoomAct = m_viewMenu->addAction("重置缩放");
    resetZoomAct->setShortcut(QKeySequence("Ctrl+0"));
    connect(resetZoomAct, &QAction::triggered, this, &MainWindow::onResetZoom);

    // ─── 工具 ───────────────────────────────────────────────────────────
    auto* toolMenu = menuBar->addMenu("工具(&T)");
    auto* settingsAct = toolMenu->addAction("设置");
    settingsAct->setShortcut(QKeySequence("Ctrl+,"));
    connect(settingsAct, &QAction::triggered, this, &MainWindow::onSettings);

    // ─── 帮助 ───────────────────────────────────────────────────────────
    auto* helpMenu = menuBar->addMenu("帮助(&H)");
    auto* aboutAct = helpMenu->addAction("关于 DocMind AI");
    connect(aboutAct, &QAction::triggered, this, &MainWindow::onAbout);
}

void MainWindow::setupToolBar() {
    auto* toolbar = addToolBar("主工具栏");
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(20, 20));

    toolbar->addAction("新建", this, &MainWindow::onNewFile);
    toolbar->addAction("打开", this, &MainWindow::onOpenFile);
    toolbar->addAction("保存", this, &MainWindow::onSave);
    toolbar->addSeparator();
    toolbar->addAction("撤销", this, &MainWindow::onUndo);
    toolbar->addAction("重做", this, &MainWindow::onRedo);
    toolbar->addSeparator();
    toolbar->addAction("查找", this, &MainWindow::onFind);
    toolbar->addSeparator();
    toolbar->addAction("编辑器", this, &MainWindow::onToggleEditorOnly);
    toolbar->addAction("分栏", this, &MainWindow::onToggleSplitView);
    toolbar->addAction("预览", this, &MainWindow::onTogglePreviewOnly);
    toolbar->addSeparator();
    toolbar->addAction("主题", this, &MainWindow::onToggleTheme);
}

void MainWindow::setupStatusBar() {
    auto* sb = statusBar();

    // 左侧：同步模式下拉菜单 + 缩放控制
    m_syncModeCombo = new QComboBox();
    m_syncModeCombo->addItem("同步：双向", 0);
    m_syncModeCombo->addItem("同步：编辑器→预览", 1);
    m_syncModeCombo->addItem("同步：预览→编辑器", 2);
    m_syncModeCombo->addItem("同步：关闭", 3);
    m_syncModeCombo->setToolTip("选择滚动同步模式");
    m_syncModeCombo->setCurrentIndex(0);  // 默认双向同步

    sb->addWidget(new QLabel("滚动同步:"));
    sb->addWidget(m_syncModeCombo);

    // 右侧：状态信息
    m_cursorLabel   = new QLabel("行 1, 列 1");
    m_lineLabel     = new QLabel("");
    m_encodingLabel = new QLabel("UTF-8");
    m_modifiedLabel = new QLabel("");

    m_cursorLabel->setMinimumWidth(120);
    m_encodingLabel->setMinimumWidth(60);

    sb->addPermanentWidget(m_cursorLabel);
    sb->addPermanentWidget(m_lineLabel);
    sb->addPermanentWidget(m_encodingLabel);
    sb->addPermanentWidget(m_modifiedLabel);

    // 连接同步模式下拉菜单信号
    connect(m_syncModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        auto* scrollSync = m_preview->scrollSync();
        if (!scrollSync) return;

        switch (index) {
            case 0:  // 双向同步
                scrollSync->setEditorToPreviewEnabled(true);
                scrollSync->setPreviewToEditorEnabled(true);
                break;
            case 1:  // 编辑器→预览
                scrollSync->setEditorToPreviewEnabled(true);
                scrollSync->setPreviewToEditorEnabled(false);
                break;
            case 2:  // 预览→编辑器
                scrollSync->setEditorToPreviewEnabled(false);
                scrollSync->setPreviewToEditorEnabled(true);
                break;
            case 3:  // 关闭
                scrollSync->setEditorToPreviewEnabled(false);
                scrollSync->setPreviewToEditorEnabled(false);
                break;
        }
    });

    updateStatusBar();
}

void MainWindow::setupConnections() {
    // 标签管理器
    connect(m_tabManager, &TabManager::currentTabChanged,
            this, &MainWindow::onCurrentTabChanged);

    // 自动保存
    auto& state = AppState::instance();
    connect(&state, &AppState::autoSaveSettingsChanged, this, [this]() {
        auto& s = AppState::instance();
        if (s.autoSaveEnabled()) {
            m_autoSaveTimer->setInterval(s.autoSaveInterval() * 1000);
            m_autoSaveTimer->start();
        } else {
            m_autoSaveTimer->stop();
        }
    });

    // 主题变更
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeMode mode) {
        auto* editor = m_tabManager->currentEditor();
        if (editor) {
            if (mode == ThemeMode::Dark) {
                editor->applyDarkTheme();
                m_preview->applyDarkTheme();
            } else {
                editor->applyLightTheme();
                m_preview->applyLightTheme();
            }
        }
    });

    // 视图模式变更
    connect(&state, &AppState::viewModeChanged,
            this, &MainWindow::applyViewMode);
}

void MainWindow::setupAutoSave() {
    m_autoSaveTimer = new QTimer(this);
    connect(m_autoSaveTimer, &QTimer::timeout,
            this, &MainWindow::onAutoSaveTimeout);

    auto& state = AppState::instance();
    if (state.autoSaveEnabled()) {
        m_autoSaveTimer->setInterval(state.autoSaveInterval() * 1000);
        m_autoSaveTimer->start();
    }
}

// ─── 文件操作 ────────────────────────────────────────────────────────────────

void MainWindow::openFile(const QString& filePath) {
    m_tabManager->openFile(filePath);
}

void MainWindow::onNewFile() {
    m_tabManager->addNewTab();
}

void MainWindow::onOpenFile() {
    auto* session = m_tabManager->currentSession();
    QString startDir = session ? session->baseDir() : QDir::homePath();

    QStringList files = QFileDialog::getOpenFileNames(
        this, "打开文件", startDir,
        "Markdown 文件 (*.md *.markdown *.mdown);;所有文件 (*)");

    for (const QString& file : files) {
        openFile(file);
    }
}

void MainWindow::onOpenRecentFile() {
    auto* action = qobject_cast<QAction*>(sender());
    if (action) {
        openFile(action->data().toString());
    }
}

void MainWindow::onSave() {
    m_tabManager->saveCurrentTab();
}

void MainWindow::onSaveAs() {
    m_tabManager->saveCurrentTabAs();
}

void MainWindow::onSaveAll() {
    m_tabManager->saveAllTabs();
}

void MainWindow::onCloseTab() {
    m_tabManager->closeCurrentTab();
}

void MainWindow::onCloseAllTabs() {
    m_tabManager->closeAllTabs();
}

void MainWindow::onCloseOtherTabs() {
    m_tabManager->closeOtherTabs(m_tabManager->currentIndex());
}

// ─── 编辑操作 ────────────────────────────────────────────────────────────────

void MainWindow::onUndo() {
    auto* editor = m_tabManager->currentEditor();
    if (editor) editor->undo();
}

void MainWindow::onRedo() {
    auto* editor = m_tabManager->currentEditor();
    if (editor) editor->redo();
}

void MainWindow::onCut() {
    auto* editor = m_tabManager->currentEditor();
    if (editor) editor->cut();
}

void MainWindow::onCopy() {
    auto* editor = m_tabManager->currentEditor();
    if (editor) editor->copy();
}

void MainWindow::onPaste() {
    auto* editor = m_tabManager->currentEditor();
    if (editor) editor->paste();
}

void MainWindow::onSelectAll() {
    auto* editor = m_tabManager->currentEditor();
    if (editor) editor->selectAll();
}

void MainWindow::onFind() {
    auto* editor = m_tabManager->currentEditor();
    if (!editor) return;

    if (!m_findDialog) {
        m_findDialog = new FindReplaceDialog(editor, this);
    }
    m_findDialog->showFindOnly();
}

void MainWindow::onFindReplace() {
    auto* editor = m_tabManager->currentEditor();
    if (!editor) return;

    if (!m_findDialog) {
        m_findDialog = new FindReplaceDialog(editor, this);
    }
    m_findDialog->showFindAndReplace();
}

void MainWindow::onGoToLine() {
    auto* editor = m_tabManager->currentEditor();
    if (!editor) return;

    int maxLines = editor->document()->blockCount();
    bool ok;
    int line = QInputDialog::getInt(this, "跳转到行",
        QString("行号 (1-%1):").arg(maxLines),
        editor->currentLine(), 1, maxLines, 1, &ok);

    if (ok) {
        QTextCursor cursor = editor->textCursor();
        cursor.movePosition(QTextCursor::Start);
        for (int i = 1; i < line; i++) {
            cursor.movePosition(QTextCursor::Down);
        }
        editor->setTextCursor(cursor);
        editor->centerCursor();
    }
}

// ─── Markdown 操作 ───────────────────────────────────────────────────────────

void MainWindow::onInsertHeading() {
    auto* editor = m_tabManager->currentEditor();
    if (editor) editor->insertHeading(1);
}

void MainWindow::onInsertBold() {
    auto* editor = m_tabManager->currentEditor();
    if (editor) editor->insertBold();
}

void MainWindow::onInsertItalic() {
    auto* editor = m_tabManager->currentEditor();
    if (editor) editor->insertItalic();
}

void MainWindow::onInsertCode() {
    auto* editor = m_tabManager->currentEditor();
    if (editor) editor->insertCode();
}

void MainWindow::onInsertCodeBlock() {
    auto* editor = m_tabManager->currentEditor();
    if (editor) editor->insertCodeBlock();
}

void MainWindow::onInsertLink() {
    auto* editor = m_tabManager->currentEditor();
    if (editor) editor->insertLink();
}

void MainWindow::onInsertImage() {
    auto* editor = m_tabManager->currentEditor();
    if (editor) editor->insertImage();
}

void MainWindow::onInsertTable() {
    auto* editor = m_tabManager->currentEditor();
    if (editor) editor->insertTable(3, 3);
}

void MainWindow::onInsertTaskList() {
    auto* editor = m_tabManager->currentEditor();
    if (editor) editor->insertTaskList();
}

void MainWindow::onInsertBlockquote() {
    auto* editor = m_tabManager->currentEditor();
    if (editor) editor->insertBlockquote();
}

void MainWindow::onInsertHorizontalRule() {
    auto* editor = m_tabManager->currentEditor();
    if (editor) editor->insertHorizontalRule();
}

void MainWindow::onInsertOrderedList() {
    auto* editor = m_tabManager->currentEditor();
    if (editor) editor->insertOrderedList();
}

void MainWindow::onInsertUnorderedList() {
    auto* editor = m_tabManager->currentEditor();
    if (editor) editor->insertUnorderedList();
}

// ─── 视图操作 ────────────────────────────────────────────────────────────────

void MainWindow::onToggleEditorOnly() {
    applyViewMode(ViewMode::EditorOnly);
    AppState::instance().setViewMode(ViewMode::EditorOnly);
}

void MainWindow::onToggleSplitView() {
    applyViewMode(ViewMode::SplitView);
    AppState::instance().setViewMode(ViewMode::SplitView);
}

void MainWindow::onTogglePreviewOnly() {
    applyViewMode(ViewMode::PreviewOnly);
    AppState::instance().setViewMode(ViewMode::PreviewOnly);
}

void MainWindow::onToggleTheme() {
    ThemeManager::instance().toggleTheme();
}

void MainWindow::onToggleLineNumbers() {
    auto* editor = m_tabManager->currentEditor();
    if (editor) {
        bool show = !editor->showLineNumbers();
        editor->setShowLineNumbers(show);
        AppState::instance().setShowLineNumbers(show);
    }
}

void MainWindow::onToggleWordWrap() {
    auto* editor = m_tabManager->currentEditor();
    if (editor) {
        bool wrap = !editor->wordWrap();
        editor->setWordWrap(wrap);
        AppState::instance().setWordWrap(wrap);
    }
}

void MainWindow::onZoomIn() {
    auto* editor = m_tabManager->currentEditor();
    if (editor) {
        QFont f = editor->font();
        if (f.pointSize() < 72) {
            f.setPointSize(f.pointSize() + 1);
            editor->setFont(f);
            AppState::instance().setEditorFont(f);
        }
    }
}

void MainWindow::onZoomOut() {
    auto* editor = m_tabManager->currentEditor();
    if (editor) {
        QFont f = editor->font();
        if (f.pointSize() > 6) {
            f.setPointSize(f.pointSize() - 1);
            editor->setFont(f);
            AppState::instance().setEditorFont(f);
        }
    }
}

void MainWindow::onResetZoom() {
    QFont f("Consolas", 12);
    f.setStyleHint(QFont::Monospace);
    auto* editor = m_tabManager->currentEditor();
    if (editor) editor->setFont(f);
    AppState::instance().setEditorFont(f);
}

// ─── 工具 ────────────────────────────────────────────────────────────────────

void MainWindow::onSettings() {
    SettingsDialog dialog(this);
    dialog.exec();

    // 应用设置变更
    auto& state = AppState::instance();
    auto* editor = m_tabManager->currentEditor();
    if (editor) {
        editor->setFont(state.editorFont());
        editor->setTabSize(state.tabSize());
        editor->setWordWrap(state.wordWrap());
        editor->setShowLineNumbers(state.showLineNumbers());
    }
}

void MainWindow::onAbout() {
    QMessageBox::about(this, "关于 DocMind AI",
        "<h2>DocMind AI</h2>"
        "<p>版本 " APP_VERSION "</p>"
        "<p>一款基于 C++/Qt 的桌面 Markdown 智能编辑器。</p>"
        "<hr>"
        "<p><b>核心功能</b></p>"
        "<ul>"
        "<li>Markdown 实时编辑与预览</li>"
        "<li>多标签文档管理</li>"
        "<li>语法高亮（支持 200+ 编程语言）</li>"
        "<li>自动保存与崩溃恢复</li>"
        "<li>浅色/深色主题切换</li>"
        "<li>文档导入导出（DOCX、PDF、HTML）</li>"
        "<li>AI 写作助手</li>"
        "</ul>"
        "<hr>"
        "<p>基于 Qt " QT_VERSION_STR " 构建</p>"
        "<p>© 2026 DocMind AI Team</p>");
}

// ─── 内部槽函数 ──────────────────────────────────────────────────────────────

void MainWindow::onCurrentTabChanged(DocumentSession* session) {
    if (!session) return;

    auto* editor = m_tabManager->currentEditor();
    if (editor) {
        connect(editor, &MarkdownEditor::cursorInfoChanged,
                this, &MainWindow::onCursorInfoChanged,
                Qt::UniqueConnection);
        connect(editor, &MarkdownEditor::contentModified,
                this, &MainWindow::onEditorContentModified,
                Qt::UniqueConnection);

        // 绑定滚动同步
        m_preview->scrollSync()->bind(editor, m_preview);

        // 初始预览
        updatePreview();

        // 应用主题
        auto& theme = ThemeManager::instance();
        if (theme.currentTheme() == ThemeMode::Dark) {
            editor->applyDarkTheme();
        } else {
            editor->applyLightTheme();
        }
    }

    updateStatusBar();
}

void MainWindow::onCursorInfoChanged(int line, int col, int selLen) {
    m_cursorLabel->setText(QString("行 %1, 列 %2").arg(line).arg(col));
    if (selLen > 0) {
        m_lineLabel->setText(QString("(已选择 %1 字符)").arg(selLen));
    } else {
        m_lineLabel->setText("");
    }
}

void MainWindow::onEditorContentModified() {
    updatePreview();
}

void MainWindow::onAutoSaveTimeout() {
    for (int i = 0; i < m_tabManager->tabCount(); i++) {
        auto* session = m_tabManager->sessionAt(i);
        if (session && session->isModified() && !session->isNewFile()) {
            QString error;
            session->saveFile(&error);
        }
    }
}

void MainWindow::updateStatusBar() {
    auto* session = m_tabManager->currentSession();
    if (!session) return;

    auto* editor = m_tabManager->currentEditor();
    if (editor) {
        auto state = editor->currentState();
        m_cursorLabel->setText(
            QString("行 %1, 列 %2").arg(state.cursorLine).arg(state.cursorColumn));
    }

    switch (session->encoding()) {
        case FileEncoding::UTF8:  m_encodingLabel->setText("UTF-8"); break;
        case FileEncoding::UTF16: m_encodingLabel->setText("UTF-16"); break;
        case FileEncoding::Latin1:m_encodingLabel->setText("Latin-1"); break;
        default: m_encodingLabel->setText("UTF-8"); break;
    }

    if (session->isModified()) {
        m_modifiedLabel->setText("● 已修改");
        m_modifiedLabel->setStyleSheet("color: #e66100;");
    } else {
        m_modifiedLabel->setText("");
    }
}

void MainWindow::updatePreview() {
    auto* editor = m_tabManager->currentEditor();
    if (!editor) return;

    auto* session = m_tabManager->currentSession();
    QString baseDir = session ? session->baseDir() : "";

    // 直接从编辑器读取内容（最可靠）
    QString content = editor->toPlainText();
    m_preview->setMarkdownContent(content, baseDir);
}

// ─── 视图模式 ────────────────────────────────────────────────────────────────

void MainWindow::applyViewMode(ViewMode mode) {
    m_currentViewMode = mode;

    switch (mode) {
        case ViewMode::EditorOnly:
            m_tabManager->show();
            m_preview->hide();
            m_splitter->setSizes({1, 0});
            break;
        case ViewMode::SplitView:
            m_tabManager->show();
            m_preview->show();
            m_splitter->setSizes({1, 1});
            break;
        case ViewMode::PreviewOnly:
            m_tabManager->hide();
            m_preview->show();
            m_splitter->setSizes({0, 1});
            break;
    }
}

// ─── 事件 ────────────────────────────────────────────────────────────────────

void MainWindow::closeEvent(QCloseEvent* event) {
    // 检查是否有未保存的文件
    bool hasUnsaved = false;
    for (int i = 0; i < m_tabManager->tabCount(); i++) {
        auto* session = m_tabManager->sessionAt(i);
        if (session && session->isModified()) {
            hasUnsaved = true;
            break;
        }
    }

    if (hasUnsaved) {
        auto result = QMessageBox::question(
            this, "退出确认",
            "有未保存的文件，是否保存后退出？",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save);

        if (result == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
        if (result == QMessageBox::Save) {
            m_tabManager->saveAllTabs();
        }
    }

    emit windowStateChanged();
    LOG_INFO("MainWindow", "主窗口关闭");
    event->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        for (const QUrl& url : mimeData->urls()) {
            if (url.isLocalFile()) {
                openFile(url.toLocalFile());
            }
        }
    }
}

} // namespace dmc
