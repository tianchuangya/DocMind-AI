// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — Logger 实现
// ─────────────────────────────────────────────────────────────────────────────
#include "utils/Logger.h"

#include <QDir>
#include <QTextStream>
#include <QCoreApplication>
#include <iostream>

namespace dmc {

Logger& Logger::instance() {
    static Logger s_instance;
    return s_instance;
}

Logger::~Logger() {
    shutdown();
}

void Logger::init(const QString& logDir, LogLevel minLevel) {
    QMutexLocker locker(&m_mutex);
    if (m_initialized) return;

    m_logDir  = logDir;
    m_minLevel = minLevel;

    QDir dir(logDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    openNewFile();
    m_initialized = true;

    info("Logger", QString("日志系统已初始化，目录: %1").arg(logDir));
}

void Logger::shutdown() {
    QMutexLocker locker(&m_mutex);
    if (m_file && m_file->isOpen()) {
        m_file->flush();
        m_file->close();
    }
    m_file.reset();
    m_initialized = false;
}

void Logger::log(LogLevel level, const QString& module, const QString& message) {
    if (level < m_minLevel) return;

    QMutexLocker locker(&m_mutex);

    // 按日期滚动日志文件
    QString today = currentDateString();
    if (m_currentDate != today) {
        openNewFile();
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString levelStr  = levelString(level);
    QString line = QString("[%1] [%2] [%3] %4\n")
                       .arg(timestamp, levelStr, module, message);

    // 写入文件
    if (m_file && m_file->isOpen()) {
        QTextStream stream(m_file.get());
        stream << line;
        stream.flush();
    }

    // 同时输出到控制台
    std::cerr << line.toStdString();
}

void Logger::openNewFile() {
    // 关闭旧文件
    if (m_file && m_file->isOpen()) {
        m_file->flush();
        m_file->close();
    }

    m_currentDate = currentDateString();
    QString filePath = QDir(m_logDir).filePath(
        QString("docmind_%1.log").arg(m_currentDate));

    m_file = std::make_unique<QFile>(filePath);
    m_file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}

QString Logger::levelString(LogLevel level) const {
    switch (level) {
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO ";
        case LogLevel::Warning: return "WARN ";
        case LogLevel::Error:   return "ERROR";
        case LogLevel::Fatal:   return "FATAL";
    }
    return "?????";
}

QString Logger::currentDateString() const {
    return QDate::currentDate().toString("yyyy-MM-dd");
}

} // namespace dmc
