// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — Application
// 应用主类：初始化、全局事件处理、单例管理
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <QApplication>
#include <memory>

namespace dmc {

class MainWindow;

class Application : public QApplication {
    Q_OBJECT

public:
    Application(int& argc, char** argv);
    ~Application() override;

    /// 初始化应用（日志、设置、主题等）
    bool initialize();

    /// 显示主窗口
    int run();

    /// 获取主窗口
    MainWindow* mainWindow() const { return m_mainWindow; }

    /// 处理命令行参数
    void processArguments();

    /// 获取待打开的文件列表
    QStringList filesToOpen() const { return m_filesToOpen; }

protected:
    bool event(QEvent* event) override;

private:
    void setupApplicationInfo();
    void initLogger();
    void initSettings();
    void initTheme();
    void createMainWindow();

    MainWindow* m_mainWindow = nullptr;
    QStringList m_filesToOpen;
    bool m_initialized = false;
};

} // namespace dmc
