#include "LogService.h"
#include <QDir>
#include <QStandardPaths>
#include <QDebug>

LogService& LogService::instance() {
    static LogService inst;
    return inst;
}

LogService::LogService(QObject* parent) : QObject(parent) {
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appData);
    QString logPath = appData + "/app.log";
    setLogFilePath(logPath);
}

LogService::~LogService() {
    QMutexLocker locker(&m_mutex);
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
}

void LogService::setLogFilePath(const QString& path) {
    QMutexLocker locker(&m_mutex);
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
    m_logFile.setFileName(path);
    if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        m_fileStream.setDevice(&m_logFile);
    }
}

QString LogService::levelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::Debug:  return "DEBUG";
        case LogLevel::Info:   return "INFO";
        case LogLevel::Warn:   return "WARN";
        case LogLevel::Error:  return "ERROR";
        case LogLevel::Packet: return "PACKET";
    }
    return "UNKNOWN";
}

void LogService::log(LogLevel level, const QString& module, const QString& message) {
    QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString levelStr = levelToString(level);
    QString line = QString("[%1] [%2] [%3] %4").arg(timeStr, levelStr, module, message);

    {
        QMutexLocker locker(&m_mutex);
        if (m_logFile.isOpen()) {
            m_fileStream << line << "\n";
            m_fileStream.flush();
        }
    }

    qDebug().noquote() << line;
    emit logAppended(level, timeStr, module, message);
}

void LogService::info(const QString& module, const QString& message) {
    log(LogLevel::Info, module, message);
}

void LogService::warn(const QString& module, const QString& message) {
    log(LogLevel::Warn, module, message);
}

void LogService::error(const QString& module, const QString& message) {
    log(LogLevel::Error, module, message);
}

void LogService::debug(const QString& module, const QString& message) {
    log(LogLevel::Debug, module, message);
}

void LogService::packet(const QString& module, const QString& message) {
    log(LogLevel::Packet, module, message);
}
