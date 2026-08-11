#ifndef OPSSCHEDULER_H
#define OPSSCHEDULER_H

#include <QObject>
#include <QDateTime>
#include <QJsonObject>

#include "ops/backupmanager.h"

class QTimer;
class QThread;
class BackupWorker;

class OpsScheduler : public QObject
{
    Q_OBJECT

public:
    explicit OpsScheduler(const QJsonObject &config, QObject *parent = nullptr);
    ~OpsScheduler() override;

    void start();

    bool backupEnabled() const;
    bool integrityEnabled() const;
    int backupIntervalHours() const;
    int integrityIntervalHours() const;
    QString backupDirectory() const;
    int retentionCount() const;

    void resetLastBackup();
    void resetIntegrityCheck();

public slots:
    void runIntegrityCheck();

signals:
    void backupFinished(bool ok, const QString &filePath, const QString &message);
    void integrityFinished(bool ok, const QString &summary);
    void backupRequested(const QString &filePath, const QString &connectionPassword, const QString &passphrase);

private slots:
    void checkSchedule();
    void onBackupWorkerFinished(const BackupManager::BackupResult &result);

private:
    void runScheduledBackup();
    void ensureBackupWorker();
    void enforceRetention();
    void readConfig(const QJsonObject &config);
    void rescheduleTimer();

    QTimer *m_timer = nullptr;
    QThread *m_backupThread = nullptr;
    BackupWorker *m_backupWorker = nullptr;
    bool m_backupInProgress = false;

    bool m_backupEnabled = false;
    bool m_integrityEnabled = false;
    int m_backupIntervalSec = 24 * 3600;
    int m_integrityIntervalSec = 24 * 3600;
    QString m_backupDirectory;
    int m_retentionCount = 14;
    // Отдельная passphrase для шифрования бэкапов (не пароль БД).
    QString m_backupPassphrase;

    QDateTime m_lastBackupAt;
    QDateTime m_lastIntegrityAt;
    bool m_initialIntegrityScheduled = false;
};

#endif // OPSSCHEDULER_H
