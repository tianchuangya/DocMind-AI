// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — 程序入口
// ─────────────────────────────────────────────────────────────────────────────
#include "app/Application.h"
#include <QGuiApplication>
#include <iostream>
#include <exception>

// WIN32 应用需要手动将 qDebug 输出到 stderr
void messageHandler(QtMsgType type, const QMessageLogContext&, const QString& msg) {
    const char* prefix = "";
    switch (type) {
        case QtDebugMsg:    prefix = "[DEBUG]"; break;
        case QtInfoMsg:     prefix = "[INFO] "; break;
        case QtWarningMsg:  prefix = "[WARN] "; break;
        case QtCriticalMsg: prefix = "[CRIT] "; break;
        case QtFatalMsg:    prefix = "[FATAL]"; break;
    }
    std::cerr << prefix << " " << msg.toStdString() << std::endl;
    if (type == QtFatalMsg) std::abort();
}

int main(int argc, char* argv[]) {
    qInstallMessageHandler(messageHandler);

    // 高 DPI 支持（必须在 QApplication 构造之前调用）
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    dmc::Application app(argc, argv);

    if (!app.initialize()) {
        return 1;
    }

    try {
        return app.run();
    } catch (const std::exception& e) {
        std::cerr << "未捕获异常: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "未捕获的未知异常！" << std::endl;
        return 1;
    }
}
