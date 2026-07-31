#include "opsscheduler.h"

#include "backupmanager.h"
#include "database/databasemanager.h"
#include "opslog.h"
#include "utils/logging.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTimer>
#include <QVariant>

static const int kSchedulerTickMs = 60000;

namespace {

struct IntegrityIssue {
    QString description;
    int count = 0;
    QString details;
};

bool countProblem(QSqlDatabase &db, const QString &label, const QString &sql, IntegrityIssue &issue)
{
    QSqlQuery q(db);
    if (!q.exec(sql)) {
        OpsLog::instance().warning(QString("Проверка целостности: не удалось выполнить запрос «%1»: %2")
                                       .arg(label, q.lastError().text()));
        return false;
    }

    int count = 0;
    QStringList sample;
    while (q.next()) {
        ++count;
        if (sample.size() < 5) {
            QStringList fields;
            for (int i = 0; i < q.record().count(); ++i)
                fields << q.value(i).toString();
            sample << fields.join(" | ");
        }
    }
    if (count > 0) {
        issue.description = label;
        issue.count = count;
        issue.details = sample.join("; ");
    }
    return true;
}

} // namespace

OpsScheduler::OpsScheduler(const QJsonObject &config, QObject *parent)
    : QObject(parent)
{
    readConfig(config);
    m_lastBackupAt = QDateTime::currentDateTime();
    m_timer = new QTimer(this);
    m_timer->setInterval(kSchedulerTickMs);
    connect(m_timer, &QTimer::timeout, this, &OpsScheduler::checkSchedule);
}

void OpsScheduler::start()
{
    m_timer->start();
    qCInfo(logApp) << "OpsScheduler: бэкапы" << (m_backupEnabled ? "включены" : "выключены")
                   << ", интервал" << (m_backupIntervalSec / 3600) << "ч, retention" << m_retentionCount;
    qCInfo(logApp) << "OpsScheduler: проверка целостности" << (m_integrityEnabled ? "включена" : "выключена")
                   << ", интервал" << (m_integrityIntervalSec / 3600) << "ч";
}

bool OpsScheduler::backupEnabled() const
{
    return m_backupEnabled;
}

bool OpsScheduler::integrityEnabled() const
{
    return m_integrityEnabled;
}

int OpsScheduler::backupIntervalHours() const
{
    return m_backupIntervalSec / 3600;
}

int OpsScheduler::integrityIntervalHours() const
{
    return m_integrityIntervalSec / 3600;
}

QString OpsScheduler::backupDirectory() const
{
    return m_backupDirectory;
}

int OpsScheduler::retentionCount() const
{
    return m_retentionCount;
}

void OpsScheduler::resetLastBackup()
{
    m_lastBackupAt = QDateTime::currentDateTime();
}

void OpsScheduler::resetIntegrityCheck()
{
    m_lastIntegrityAt = QDateTime::currentDateTime();
}

void OpsScheduler::readConfig(const QJsonObject &config)
{
    QJsonObject backup = config["backup"].toObject();
    m_backupEnabled = backup["enabled"].toBool(false);
    int hours = backup["interval_hours"].toInt(24);
    m_backupIntervalSec = qMax(1, hours) * 3600;
    m_backupDirectory = backup["directory"].toString().trimmed();
    m_retentionCount = qMax(0, backup["retention_count"].toInt(14));

    QJsonObject monitoring = config["monitoring"].toObject();
    m_integrityEnabled = monitoring["integrity_enabled"].toBool(false);
    int integrityHours = monitoring["integrity_interval_hours"].toInt(24);
    m_integrityIntervalSec = qMax(1, integrityHours) * 3600;

    // Каталог бэкапов: если пусто — <appdir>/backups
    if (m_backupDirectory.isEmpty()) {
        m_backupDirectory = QCoreApplication::applicationDirPath() + "/backups";
    } else if (QDir::isRelativePath(m_backupDirectory)) {
        m_backupDirectory = QCoreApplication::applicationDirPath() + "/" + m_backupDirectory;
    }
}

void OpsScheduler::checkSchedule()
{
    QDateTime now = QDateTime::currentDateTime();

    if (m_backupEnabled && (m_lastBackupAt.secsTo(now) >= m_backupIntervalSec)) {
        bool ok = runScheduledBackup();
        m_lastBackupAt = ok ? now : now.addSecs(-qMax(0, m_backupIntervalSec - 600));
    }

    if (m_integrityEnabled && (m_lastIntegrityAt.isNull() || m_lastIntegrityAt.secsTo(now) >= m_integrityIntervalSec)) {
        m_lastIntegrityAt = now;
        runIntegrityCheck();
    }
}

bool OpsScheduler::runScheduledBackup()
{
    if (!DatabaseManager::instance().isConnected()) {
        OpsLog::instance().error("Автоматический бэкап пропущен: нет подключения к БД");
        emit backupFinished(false, QString(), "Нет подключения к БД");
        return false;
    }

    QDir dir(m_backupDirectory);
    if (!dir.mkpath(".")) {
        OpsLog::instance().error("Автоматический бэкап не выполнен: не удалось создать каталог " + m_backupDirectory);
        emit backupFinished(false, QString(), "Не удалось создать каталог бэкапов");
        return false;
    }

    QString filePath = dir.absoluteFilePath(
        QString("backup_poc_%1.sql").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")));

    QString password = DatabaseManager::instance().getDatabase().password();
    BackupManager::BackupResult result = BackupManager::createBackup(
        DatabaseManager::instance().getDatabase(), filePath, password);

    QString message;
    if (result.ok) {
        message = QString("Автоматический бэкап создан (метод: %1): %2, размер %3 KB")
                      .arg(result.method, filePath)
                      .arg(QString::number(result.size / 1024));
        OpsLog::instance().info(message);
        enforceRetention();
    } else {
        message = QString("Автоматический бэкап не удался: %1").arg(result.error);
        OpsLog::instance().error(message);
    }

    emit backupFinished(result.ok, filePath, message);
    return result.ok;
}

void OpsScheduler::enforceRetention()
{
    if (m_retentionCount <= 0)
        return;

    QStringList files = QDir(m_backupDirectory).entryList({"backup_poc_*.sql"}, QDir::Files, QDir::Name);
    while (files.size() > m_retentionCount) {
        QString oldest = files.takeFirst();
        QString path = QDir(m_backupDirectory).filePath(oldest);
        if (QFile::remove(path)) {
            OpsLog::instance().info(QString("Ретенция: удалён старый бэкап %1").arg(path));
        } else {
            OpsLog::instance().warning(QString("Ретенция: не удалось удалить %1").arg(path));
        }
    }
}

void OpsScheduler::runIntegrityCheck()
{
    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    if (!db.isOpen()) {
        OpsLog::instance().error("Проверка целостности не выполнена: нет подключения к БД");
        emit integrityFinished(false, "Нет подключения к БД");
        return;
    }

    IntegrityIssue issues[3];
    countProblem(db, "Терминал свободен, но SIM в аренде",
                 "SELECT t.terminalid, t.serialnumber, s.simcardid, s.simnumber "
                 "FROM tblterminals t "
                 "JOIN tblsimcards s ON t.currentsimcardid = s.simcardid "
                 "WHERE t.status = 0 AND s.status = 1",
                 issues[0]);
    countProblem(db, "SIM в аренде, но не привязана к терминалу",
                 "SELECT s.simcardid, s.simnumber "
                 "FROM tblsimcards s "
                 "LEFT JOIN tblterminals t ON s.simcardid = t.currentsimcardid "
                 "WHERE s.status = 1 AND t.terminalid IS NULL",
                 issues[1]);
    countProblem(db, "Терминал в аренде, но нет SIM",
                 "SELECT t.terminalid, t.serialnumber "
                 "FROM tblterminals t "
                 "WHERE t.status = 1 AND t.currentsimcardid IS NULL",
                 issues[2]);

    QString summary;
    bool ok = true;
    int total = 0;
    for (const IntegrityIssue &issue : issues) {
        if (issue.count > 0) {
            ok = false;
            total += issue.count;
            summary += QString("\n  • %1 — %2 шт. (примеры: %3)")
                           .arg(issue.description)
                           .arg(issue.count)
                           .arg(issue.details);
            OpsLog::instance().warning(QString("Найдено несоответствие: %1 — %2 шт. (примеры: %3)")
                                           .arg(issue.description)
                                           .arg(issue.count)
                                           .arg(issue.details));
        }
    }

    if (ok) {
        summary = "Целостность БД: замечаний нет";
        OpsLog::instance().info(summary);
    } else {
        summary = QString("Целостность БД: обнаружено %1 несоответствий") .arg(total) + summary;
        OpsLog::instance().error(summary);
    }

    emit integrityFinished(ok, summary);
}
