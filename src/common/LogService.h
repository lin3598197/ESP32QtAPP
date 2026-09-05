#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QMutex>

enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error,
    Packet
};

class LogService : public QObject {
    Q_OBJECT
public:
    static LogService& instance();

    void log(LogLevel level, const QString& module, const QString& message);
    void info(const QString& module, const QString& message);
    void warn(const QString& module, const QString& message);
    void error(const QString& module, const QString& message);
    void debug(const QString& module, const QString& message);
    void packet(const QString& module, const QString& message);

    void setLogFilePath(const QString& path);

signals:
    void logAppended(LogLevel level, const QString& timestamp, const QString& module, const QString& message);

private:
    LogService(QObject* parent = nullptr);
    ~LogService();
    LogService(const LogService&) = delete;
    LogService& operator=(const LogService&) = delete;

    QString levelToString(LogLevel level) const;

    QMutex m_mutex;
    QFile m_logFile;
    QTextStream m_fileStream;
};
