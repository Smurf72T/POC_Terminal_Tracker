#include "test_db_integration.h"

#include "ops/backupmanager.h"
#include "ops/opslog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QStandardPaths>
#include <QtTest>

void TestDbIntegration::test_backup_and_opslog()
{
    QString tempDir = QDir::temp().filePath("poc_ops_test");
    QDir(tempDir).mkpath(".");
    QString dbname = m_testDb.databaseName();

    // Fallback-дамп не зависит от внешних утилит и должен содержать структуру и данные
    QString err;
    QString backupFile = tempDir + "/backup_test.sql";
    QVERIFY2(BackupManager::createFallbackBackup(m_testDb, backupFile, dbname, &err), qPrintable(err));
    QVERIFY2(QFileInfo(backupFile).size() > 0, qPrintable("fallback-дамп пустой"));
    QFile f(backupFile);
    QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
    QString content = QString::fromUtf8(f.readAll());
    f.close();
    QVERIFY2(content.contains("CREATE TABLE"), qPrintable("дамп не содержит CREATE TABLE"));
    QVERIFY2(content.contains("INSERT INTO"), qPrintable("дамп не содержит данных"));

    // createBackup: pg_dump (если доступен в PATH) либо fallback — в любом случае файл создаётся
    QString backupFull = tempDir + "/backup_full.sql";
    BackupManager::BackupResult result =
        BackupManager::createBackup(m_testDb, backupFull, m_testDb.password(), m_testDb.password());
    QVERIFY2(result.ok, qPrintable(result.error));
    QVERIFY2(result.size > 0, qPrintable("бэкап пустой"));
    QVERIFY2(result.method == "pg_dump" || result.method == "fallback",
             qPrintable("неизвестный метод бэкапа: " + result.method));

    // Шифрование: бэкап, созданный с паролем, начинается с маркера POCENC1
    QVERIFY2(result.encrypted, qPrintable("бэкап с паролем должен быть зашифрован"));
    QFile enc(backupFull);
    QVERIFY2(enc.open(QIODevice::ReadOnly), qPrintable("не удалось открыть зашифрованный бэкап"));
    QCOMPARE(QString::fromUtf8(enc.read(8)), QString("POCENC1\n"));
    enc.close();

    // Roundtrip: восстановление зашифрованного бэкапа в СВЕЖУЮ пустую БД.
    // (Восстановление поверх рабочей БД не поддерживается: pg_dump --clean
    //  не удаляет таблицы с внешнеключевыми зависимостями.)
    // Восстановление выполняется внешней утилитой psql — если её нет в PATH,
    // пропускаем roundtrip-часть вместо падения (локально, без PostgreSQL).
    if (QStandardPaths::findExecutable("psql").isEmpty()) {
        QSKIP("psql не найден в PATH — roundtrip-проверка восстановления пропущена");
    }
    const QString restoreDbName = "pocbase_test_restore";
    QSqlQuery termR(m_adminDb);
    termR.exec(QString("SELECT pg_terminate_backend(pid) FROM pg_stat_activity "
                       "WHERE datname = '%1' AND pid <> pg_backend_pid()")
                   .arg(restoreDbName));
    QSqlQuery dropR(m_adminDb);
    QVERIFY2(dropR.exec(QString("DROP DATABASE IF EXISTS %1").arg(restoreDbName)),
             qPrintable(dropR.lastError().text()));
    QSqlQuery createR(m_adminDb);
    QVERIFY2(createR.exec(QString("CREATE DATABASE %1 ENCODING 'UTF8'").arg(restoreDbName)),
             qPrintable(createR.lastError().text()));

    {
        QSqlDatabase restoreDb = QSqlDatabase::addDatabase("QPSQL", "restoreConnection");
        QString rErr;
        QVERIFY2(openConnection(restoreDb, restoreDbName, m_env, m_dbConfig, &rErr), qPrintable(rErr));
        QString restoreErr;
        QVERIFY2(BackupManager::restoreDatabase(restoreDb, backupFull, m_testDb.password(), m_testDb.password(),
                                                &restoreErr),
                 qPrintable(restoreErr));
        restoreDb.close();
        QSqlDatabase::removeDatabase("restoreConnection");
    }

    // В восстановленной БД должны быть схема (миграции) и данные
    {
        QSqlDatabase check = QSqlDatabase::addDatabase("QPSQL", "restoreCheckConnection");
        QString cErr;
        QVERIFY2(openConnection(check, restoreDbName, m_env, m_dbConfig, &cErr), qPrintable(cErr));
        QSqlQuery q(check);
        QVERIFY2(q.exec("SELECT count(*) FROM tbl_users"), qPrintable(q.lastError().text()));
        QVERIFY(q.next());
        QVERIFY2(q.value(0).toInt() > 0, qPrintable("в восстановленной БД нет пользователей"));
        QSqlQuery mq(check);
        QVERIFY(mq.exec("SELECT count(*) FROM schema_migrations"));
        QVERIFY(mq.next());
        QCOMPARE(mq.value(0).toInt(), 14);
        check.close();
        QSqlDatabase::removeDatabase("restoreCheckConnection");
    }

    QSqlQuery termR2(m_adminDb);
    termR2.exec(QString("SELECT pg_terminate_backend(pid) FROM pg_stat_activity "
                        "WHERE datname = '%1' AND pid <> pg_backend_pid()")
                    .arg(restoreDbName));
    QSqlQuery dropR2(m_adminDb);
    dropR2.exec(QString("DROP DATABASE IF EXISTS %1").arg(restoreDbName));

    // Бэкап без пароля — plaintext SQL (обратная совместимость)
    QString backupPlain = tempDir + "/backup_plain.sql";
    BackupManager::BackupResult plainResult =
        BackupManager::createBackup(m_testDb, backupPlain, m_testDb.password(), QString());
    QVERIFY2(plainResult.ok, qPrintable(plainResult.error));
    QVERIFY2(!plainResult.encrypted, qPrintable("бэкап без пароля не должен быть зашифрован"));
    QFile pf(backupPlain);
    QVERIFY(pf.open(QIODevice::ReadOnly | QIODevice::Text));
    QString plainContent = QString::fromUtf8(pf.readAll());
    pf.close();
    QVERIFY2(plainContent.contains("CREATE TABLE"), qPrintable("plain-бэкап не содержит CREATE TABLE"));

    // Журнал операций: запись должна появиться в ops.log
    OpsLog::instance().setLogDirectory(tempDir);
    QString msg =
        "Тестовая запись журнала операций " + QString::number(QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
    OpsLog::instance().info(msg);
    QString logPath = OpsLog::instance().logFilePath();
    QVERIFY2(QFile::exists(logPath), qPrintable("ops.log не создан: " + logPath));
    QFile lf(logPath);
    QVERIFY(lf.open(QIODevice::ReadOnly | QIODevice::Text));
    QString logContent = QString::fromUtf8(lf.readAll());
    lf.close();
    QVERIFY2(logContent.contains(msg), qPrintable("ops.log не содержит запись"));

    QDir(tempDir).removeRecursively();
}