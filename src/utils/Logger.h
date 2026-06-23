// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — Logger
// 轻量级日志系统，支持控制台与文件输出，按日期滚动
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <QString>
#include <QFile>
#include <QRecursiveMutex>
#include <QDateTime>
#include <memory>

namespace dmc {

// ─── 日志等级 ────────────────────────────────────────────────────────────────
enum class LogLevel {
    Debug   = 0,
    Info    = 1,
    Warning = 2,
    Error   = 3,
    Fatal   = 4,
};

// ─── 日志器（单例） ─────────────────────────────────────────────────────────
class Logger {
public:
    static Logger& instance();

    /// 初始化日志系统：设置日志目录与最低输出级别
    void init(const QString& logDir, LogLevel minLevel = LogLevel::Info);

    /// 写一条日志
    void log(LogLevel level, const QString& module, const QString& message);

    /// 关闭当前日志文件
    void shutdown();

    /// 便捷宏辅助
    void debug  (const QString& module, const QString& msg) { log(LogLevel::Debug,   module, msg); }
    void info   (const QString& module, const QString& msg) { log(LogLevel::Info,    module, msg); }
    void warning(const QString& module, const QString& msg) { log(LogLevel::Warning, module, msg); }
    void error  (const QString& module, const QString& msg) { log(LogLevel::Error,   module, msg); }
    void fatal  (const QString& module, const QString& msg) { log(LogLevel::Fatal,   module, msg); }

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void openNewFile();
    QString levelString(LogLevel level) const;
    QString currentDateString() const;

    QString          m_logDir;
    QString          m_currentDate;
    LogLevel         m_minLevel = LogLevel::Info;
    std::unique_ptr<QFile> m_file;
    QRecursiveMutex  m_mutex;
    bool             m_initialized = false;
};

} // namespace dmc

// ─── 宏 ─────────────────────────────────────────────────────────────────────
#define LOG_DEBUG(mod, msg)   dmc::Logger::instance().debug(mod, msg)
#define LOG_INFO(mod, msg)    dmc::Logger::instance().info(mod, msg)
#define LOG_WARN(mod, msg)    dmc::Logger::instance().warning(mod, msg)
#define LOG_ERROR(mod, msg)   dmc::Logger::instance().error(mod, msg)
#define LOG_FATAL(mod, msg)   dmc::Logger::instance().fatal(mod, msg)
