#include "opsscheduler.h"

#include "backupmanager.h"
#include "backupworker.h"
#include "database/databasemanager.h"
#include "opslog.h"
#include "utils/logging.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QThread>
#include <QTimer>
#include <QVariant>

static const int kSchedulerMinTickMs = 5000;
static const int kSchedulerMaxTickMs = 60000;
static const int kSchedulerInitialTickMs = 10000;

namespace {

struct IntegrityIssue {
    QString description;
    int count = 0;
    QString details;
};

bool countProblem(QSqlDatabase& db, const QString& label, const QString& sql, IntegrityIssue& issue)
{
    QSqlQuery q(db);
    if (!q.exec(sql)) {
        OpsLog::instance().warning(
            QString("Проверка целостности: не удалось выполнить запрос «%1»: %2").arg(label, q.lastError().text()));
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

OpsScheduler::OpsScheduler(const QJsonObject& config, QObject* parent) : QObject(parent)
{
    readConfig(config);
    m_lastBackupAt = QDateTime::currentDateTime();
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &OpsScheduler::checkSchedule);
}

OpsScheduler::~OpsScheduler()
{
    if (m_backupThread) {
        m_backupThread->quit();
        if (!m_backupThread->wait(5000)) {
            qCWarning(logApp) << "OpsScheduler: не дождались завершения потока за 5 c, запрашиваем отмену";
            OpsLog::instance().warning("Не дождались завершения потока бэкапа за 5 c — запрошена отмена");
            m_backupThread->requestInterruption();
            m_backupWorker->requestCancel();
            m_backupThread->wait(5000);
        }
        delete m_backupWorker;
        delete m_backupThread;
        m_backupWorker = nullptr;
        m_backupThread = nullptr;
    }
}

void OpsScheduler::start()
{
    m_timer->start(kSchedulerInitialTickMs);
    qCInfo(logApp) << "OpsScheduler: бэкапы" << (m_backupEnabled ? "включены" : "выключены") << ", интервал"
                   << (m_backupIntervalSec / 3600) << "ч, retention" << m_retentionCount;
    qCInfo(logApp) << "OpsScheduler: проверка целостности" << (m_integrityEnabled ? "включена" : "выключена")
                   << ", интервал" << (m_integrityIntervalSec / 3600) << "ч";

    // Автобэкап без passphrase сохраняет резервную копию незашифрованной —
    // предупреждаем сразу, а не в момент первого дампа.
    if (m_backupEnabled && m_backupPassphrase.isEmpty()) {
        qCWarning(logApp) << "OpsScheduler: автобэкап включён, но backup.passphrase пуст —"
                          << "резервные копии не будут шифроваться. Задайте passphrase в config.json.";
        OpsLog::instance().warning(
            "Автобэкап без passphrase: резервные копии не шифруются (config backup.passphrase).");
    }
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

void OpsScheduler::readConfig(const QJsonObject& config)
{
    QJsonObject backup = config["backup"].toObject();
    m_backupEnabled = backup["enabled"].toBool(false);
    int hours = backup["interval_hours"].toInt(24);
    m_backupIntervalSec = qMax(1, hours) * 3600;
    m_backupDirectory = backup["directory"].toString().trimmed();
    m_retentionCount = qMax(0, backup["retention_count"].toInt(14));
    m_backupPassphrase = backup["passphrase"].toString();

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

    if (m_backupEnabled && !m_backupInProgress && (m_lastBackupAt.secsTo(now) >= m_backupIntervalSec)) {
        runScheduledBackup();
    }

    if (m_integrityEnabled && (m_lastIntegrityAt.isNull() || m_lastIntegrityAt.secsTo(now) >= m_integrityIntervalSec)) {
        m_lastIntegrityAt = now;
        runIntegrityCheck();
    }

    rescheduleTimer();
}

void OpsScheduler::rescheduleTimer()
{
    // Точное расписание: перепланируем таймер на момент ближайшего события,
    // не чаще 5 с и не реже 60 с (последнее — для реакции на ручные сбросы).
    QDateTime now = QDateTime::currentDateTime();
    qint64 nextInMs = kSchedulerMaxTickMs;

    if (m_backupEnabled && !m_backupInProgress) {
        qint64 backupInMs = m_lastBackupAt.addSecs(m_backupIntervalSec).toMSecsSinceEpoch() - now.toMSecsSinceEpoch();
        nextInMs = qMin(nextInMs, qMax<qint64>(kSchedulerMinTickMs, backupInMs));
    }

    if (m_integrityEnabled) {
        qint64 integrityInMs =
            m_lastIntegrityAt.isNull()
                ? kSchedulerMinTickMs
                : m_lastIntegrityAt.addSecs(m_integrityIntervalSec).toMSecsSinceEpoch() - now.toMSecsSinceEpoch();
        nextInMs = qMin(nextInMs, qMax<qint64>(kSchedulerMinTickMs, integrityInMs));
    }

    m_timer->start(qMin<qint64>(nextInMs, kSchedulerMaxTickMs));
}

void OpsScheduler::ensureBackupWorker()
{
    m_backupWorker = createBackupWorker(m_backupThread);
    if (!m_backupWorker)
        return;

    connect(this, &OpsScheduler::backupRequested, m_backupWorker, &BackupWorker::createBackup, Qt::QueuedConnection);
    connect(m_backupWorker, &BackupWorker::backupFinished, this, &OpsScheduler::onBackupWorkerFinished);
}

void OpsScheduler::runScheduledBackup()
{
    if (!DatabaseManager::instance().isConnected()) {
        OpsLog::instance().error("Автоматический бэкап пропущен: нет подключения к БД");
        emit backupFinished(false, QString(), "Нет подключения к БД");
        return;
    }

    QDir dir(m_backupDirectory);
    if (!dir.mkpath(".")) {
        OpsLog::instance().error("Автоматический бэкап не выполнен: не удалось создать каталог " + m_backupDirectory);
        emit backupFinished(false, QString(), "Не удалось создать каталог бэкапов");
        return;
    }

    QString filePath = dir.absoluteFilePath(
        QString("backup_poc_%1.sql").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")));

    // Для подключения к pg_dump используем пароль БД, для шифрования —
    // отдельную passphrase из конфига (backup.passphrase).
    QString connectionPassword = DatabaseManager::instance().getDatabase().password();

    ensureBackupWorker();
    m_backupInProgress = true;
    emit backupRequested(filePath, connectionPassword, m_backupPassphrase);
}

void OpsScheduler::onBackupWorkerFinished(const BackupManager::BackupResult& result)
{
    m_backupInProgress = false;

    if (result.ok) {
        m_lastBackupAt = QDateTime::currentDateTime();
        enforceRetention();
        QString message = QString("Автоматический бэкап создан (метод: %1): %2, размер %3 KB")
                              .arg(result.method, result.filePath)
                              .arg(QString::number(result.size / 1024));
        OpsLog::instance().info(message);
        emit backupFinished(true, result.filePath, message);
    } else {
        m_lastBackupAt = QDateTime::currentDateTime().addSecs(-qMax(0, m_backupIntervalSec - 600));
        QString message = QString("Автоматический бэкап не удался: %1").arg(result.error);
        OpsLog::instance().error(message);
        emit backupFinished(false, result.filePath, message);
    }

    rescheduleTimer();
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
                 "JOIN tblsimcards s ON (t.currentsimcardid = s.simcardid OR t.currentsimcardid2 = s.simcardid) "
                 "WHERE t.status = 0 AND s.status = 1",
                 issues[0]);
    countProblem(db, "SIM в аренде, но не привязана к терминалу",
                 "SELECT s.simcardid, s.simnumber "
                 "FROM tblsimcards s "
                 "LEFT JOIN tblterminals t ON (s.simcardid = t.currentsimcardid OR s.simcardid = t.currentsimcardid2) "
                 "WHERE s.status = 1 AND t.terminalid IS NULL",
                 issues[1]);
    countProblem(db, "Терминал в аренде, но нет SIM",
                 "SELECT t.terminalid, t.serialnumber "
                 "FROM tblterminals t "
                 "WHERE t.status = 1 AND t.currentsimcardid IS NULL AND t.currentsimcardid2 IS NULL",
                 issues[2]);

    QString summary;
    bool ok = true;
    int total = 0;
    for (const IntegrityIssue& issue : issues) {
        if (issue.count > 0) {
            ok = false;
            total += issue.count;
            summary +=
                QString("\n  • %1 — %2 шт. (примеры: %3)").arg(issue.description).arg(issue.count).arg(issue.details);
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
        summary = QString("Целостность БД: обнаружено %1 несоответствий").arg(total) + summary;
        OpsLog::instance().error(summary);
    }

    emit integrityFinished(ok, summary);
}
