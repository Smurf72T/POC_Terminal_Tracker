#include "opslog.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTextStream>

static QString levelToString(OpsLog::Level level)
{
    switch (level) {
    case OpsLog::Info:
        return "INFO";
    case OpsLog::Warning:
        return "WARN";
    case OpsLog::Error:
        return "ERROR";
    }
    return "INFO";
}

OpsLog& OpsLog::instance()
{
    static OpsLog instance;
    return instance;
}

void OpsLog::setLogDirectory(const QString &directory)
{
    QMutexLocker locker(&m_mutex);
    m_logDirectory = directory;
}

QString OpsLog::logFilePath() const
{
    QMutexLocker locker(&m_mutex);
    return resolveLogDirectory() + "/ops.log";
}

QString OpsLog::resolveLogDirectory() const
{
    if (!m_logDirectory.isEmpty()) {
        QDir dir(m_logDirectory);
        if (dir.mkpath("."))
            return dir.absolutePath();
    }

    QString appDir = QCoreApplication::applicationDirPath();
    QDir defaultDir(appDir + "/logs");
    if (defaultDir.mkpath("."))
        return defaultDir.absolutePath();

    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

void OpsLog::rotateIfNeeded(const QString &filePath)
{
    QFileInfo info(filePath);
    if (info.size() <= 1024 * 1024)
        return;

    QFile::remove(filePath + ".1");
    QFile::rename(filePath, filePath + ".1");
}

void OpsLog::log(Level level, const QString &message)
{
    QMutexLocker locker(&m_mutex);
    QString filePath = resolveLogDirectory() + "/ops.log";
    rotateIfNeeded(filePath);

    QFile file(filePath);
    if (!file.open(QIODevice::Append | QIODevice::WriteOnly | QIODevice::Text))
        return;

    QTextStream out(&file);
    out << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")
        << " [" << levelToString(level) << "] " << message << "\n";
    file.close();
}

void OpsLog::info(const QString &message)
{
    log(Info, message);
}

void OpsLog::warning(const QString &message)
{
    log(Warning, message);
}

void OpsLog::error(const QString &message)
{
    log(Error, message);
}
