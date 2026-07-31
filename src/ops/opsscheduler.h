#ifndef OPSSCHEDULER_H
#define OPSSCHEDULER_H

#include <QObject>
#include <QDateTime>
#include <QJsonObject>

class QTimer;

class OpsScheduler : public QObject
{
    Q_OBJECT

public:
    explicit OpsScheduler(const QJsonObject &config, QObject *parent = nullptr);

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

private slots:
    void checkSchedule();

private:
    bool runScheduledBackup();
    void enforceRetention();
    void readConfig(const QJsonObject &config);

    QTimer *m_timer = nullptr;

    bool m_backupEnabled = false;
    bool m_integrityEnabled = false;
    int m_backupIntervalSec = 24 * 3600;
    int m_integrityIntervalSec = 24 * 3600;
    QString m_backupDirectory;
    int m_retentionCount = 14;

    QDateTime m_lastBackupAt;
    QDateTime m_lastIntegrityAt;
    bool m_initialIntegrityScheduled = false;
};

#endif // OPSSCHEDULER_H
