// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — Application 实现
// ─────────────────────────────────────────────────────────────────────────────
#include "app/Application.h"
#include "app/MainWindow.h"
#include "widgets/TabManager.h"
#include "core/DocumentSession.h"
#include "core/AppState.h"
#include "widgets/ThemeManager.h"
#include "utils/Logger.h"

#include <QDebug>
#include <QFileOpenEvent>
#include <QDir>
#include <QStandardPaths>

namespace dmc {

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv)
{
    setupApplicationInfo();
    processArguments();
}

Application::~Application() {
    delete m_mainWindow;
    Logger::instance().shutdown();
}

void Application::setupApplicationInfo() {
    setApplicationName("DocMind AI");
    setApplicationDisplayName("DocMind AI");
    setOrganizationName("DocMindAI");
    setOrganizationDomain("docmind.ai");
    setApplicationVersion(APP_VERSION);
}

void Application::processArguments() {
    const QStringList args = arguments();
    for (int i = 1; i < args.size(); i++) {
        const QString& arg = args[i];
        if (!arg.startsWith('-') && QFile::exists(arg)) {
            m_filesToOpen.append(QFileInfo(arg).absoluteFilePath());
        }
    }
}

bool Application::initialize() {
    if (m_initialized) return true;

    // 初始化设置
    initSettings();

    // 初始化日志
    initLogger();

    LOG_INFO("Application", "DocMind AI v" APP_VERSION " 启动中...");

    // 初始化主题
    initTheme();

    // 创建主窗口
    createMainWindow();

    m_initialized = true;
    LOG_INFO("Application", "初始化完成");
    return true;
}

int Application::run() {
    if (!m_initialized) {
        if (!initialize()) return -1;
    }

    // 显示主窗口
    m_mainWindow->show();

    // 打开命令行传入的文件
    for (const QString& file : m_filesToOpen) {
        m_mainWindow->openFile(file);
    }

    // 如果没有打开任何文件，恢复上次会话
    if (m_mainWindow->tabManager()->tabCount() == 0) {
        auto& state = AppState::instance();
        QStringList files = state.openFilesOnStartup();
        if (!files.isEmpty()) {
            for (const QString& file : files) {
                if (QFile::exists(file)) {
                    m_mainWindow->openFile(file);
                }
            }
        } else {
            m_mainWindow->tabManager()->addNewTab();
        }
    }

    return exec();
}

void Application::initLogger() {
    QString logDir = AppState::instance().logDir();
    Logger::instance().init(logDir, LogLevel::Info);
}

void Application::initSettings() {
    AppState::instance().loadSettings();
}

void Application::initTheme() {
    ThemeManager::instance().init();
}

void Application::createMainWindow() {
    m_mainWindow = new MainWindow();

    // 恢复窗口状态
    auto& state = AppState::instance();
    if (!state.windowGeometry().isEmpty()) {
        m_mainWindow->restoreGeometry(state.windowGeometry());
    }
    if (!state.windowState().isEmpty()) {
        m_mainWindow->restoreState(state.windowState());
    }

    // 窗口关闭时保存状态
    connect(m_mainWindow, &MainWindow::windowStateChanged, this, [this]() {
        auto& state = AppState::instance();
        state.setWindowGeometry(m_mainWindow->saveGeometry());
        state.setWindowState(m_mainWindow->saveState());

        // 保存当前打开的文件
        QStringList files;
        for (int i = 0; i < m_mainWindow->tabManager()->tabCount(); i++) {
            auto* session = m_mainWindow->tabManager()->sessionAt(i);
            if (session && !session->isNewFile()) {
                files.append(session->filePath());
            }
        }
        state.setOpenFilesOnStartup(files);
        state.setLastActiveTab(m_mainWindow->tabManager()->currentIndex());
        state.saveSettings();
    });
}

bool Application::event(QEvent* event) {
    // macOS 文件打开事件
    if (event->type() == QEvent::FileOpen) {
        auto* fileEvent = static_cast<QFileOpenEvent*>(event);
        QString filePath = fileEvent->file();
        if (m_mainWindow) {
            m_mainWindow->openFile(filePath);
        } else {
            m_filesToOpen.append(filePath);
        }
        return true;
    }

    return QApplication::event(event);
}

} // namespace dmc
