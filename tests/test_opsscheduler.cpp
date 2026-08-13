#include <QTest>
#include <QJsonObject>
#include <QSignalSpy>

#include "ops/opsscheduler.h"
#include "ops/backupmanager.h"

class TestOpsScheduler : public QObject {
    Q_OBJECT

private slots:
    void configParsing();
    void disabledSchedulerEmitsNothing();
    void backupResultMetatypeRegistered();
};

static QJsonObject makeConfig(bool backupEnabled = false, int backupHours = 24, bool integrityEnabled = false,
                              int integrityHours = 24, const QString& backupDir = QString(), int retention = 14)
{
    QJsonObject backup;
    backup["enabled"] = backupEnabled;
    backup["interval_hours"] = backupHours;
    backup["directory"] = backupDir;
    backup["retention_count"] = retention;
    backup["passphrase"] = "test-passphrase";

    QJsonObject monitoring;
    monitoring["integrity_enabled"] = integrityEnabled;
    monitoring["integrity_interval_hours"] = integrityHours;

    QJsonObject cfg;
    cfg["backup"] = backup;
    cfg["monitoring"] = monitoring;
    return cfg;
}

void TestOpsScheduler::configParsing()
{
    OpsScheduler sched(makeConfig(true, 6, true, 48, "custom_dir", 5));
    QCOMPARE(sched.backupEnabled(), true);
    QCOMPARE(sched.backupIntervalHours(), 6);
    QCOMPARE(sched.integrityEnabled(), true);
    QCOMPARE(sched.integrityIntervalHours(), 48);
    QCOMPARE(sched.retentionCount(), 5);
    QVERIFY(sched.backupDirectory().endsWith("/custom_dir"));

    OpsScheduler defaults(makeConfig());
    QCOMPARE(defaults.backupEnabled(), false);
    QCOMPARE(defaults.backupIntervalHours(), 24);
    QCOMPARE(defaults.integrityEnabled(), false);
    QCOMPARE(defaults.integrityIntervalHours(), 24);
    QCOMPARE(defaults.retentionCount(), 14);
    QVERIFY(defaults.backupDirectory().endsWith("/backups"));
}

void TestOpsScheduler::disabledSchedulerEmitsNothing()
{
    OpsScheduler sched(makeConfig());
    QSignalSpy backupSpy(&sched, &OpsScheduler::backupFinished);
    QSignalSpy integritySpy(&sched, &OpsScheduler::integrityFinished);

    sched.start();
    QTest::qWait(200);

    QCOMPARE(backupSpy.count(), 0);
    QCOMPARE(integritySpy.count(), 0);
}

void TestOpsScheduler::backupResultMetatypeRegistered()
{
    // BackupResult передаётся через сигнал между потоками (queued). Если тип не
    // зарегистрирован — Qt дропает аргумент и бэкап "зависает" (m_backupInProgress).
    QVERIFY(QMetaType::isRegistered(qMetaTypeId<BackupManager::BackupResult>()));
    QVERIFY(QMetaType::type("BackupManager::BackupResult") != QMetaType::UnknownType);
}

QTEST_GUILESS_MAIN(TestOpsScheduler)

#include "test_opsscheduler.moc"
