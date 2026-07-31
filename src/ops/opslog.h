#ifndef OPSLOG_H
#define OPSLOG_H

#include <QString>
#include <QMutex>

class OpsLog
{
public:
    enum Level { Info, Warning, Error };

    static OpsLog& instance();

    void setLogDirectory(const QString &directory);
    QString logFilePath() const;

    void log(Level level, const QString &message);
    void info(const QString &message);
    void warning(const QString &message);
    void error(const QString &message);

private:
    OpsLog() = default;
    ~OpsLog() = default;
    OpsLog(const OpsLog&) = delete;
    OpsLog& operator=(const OpsLog&) = delete;

    QString resolveLogDirectory() const;
    void rotateIfNeeded(const QString &filePath);

    QString m_logDirectory;
    mutable QMutex m_mutex;
};

#endif // OPSLOG_H
