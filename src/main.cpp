// DocMind AI — 模块 C 课程设计 Demo 入口
#include "app/DemoWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QStandardPaths>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("DocMindAI"));
    app.setOrganizationName(QStringLiteral("DocMindAI"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));

    // 确保 AppData 目录存在
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataDir.isEmpty()) dataDir = QDir::homePath() + QStringLiteral("/.docmindai");
    QDir().mkpath(dataDir);

    dmc::app::DemoWindow w;
    w.show();

    return app.exec();
}
