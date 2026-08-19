#include "test_concurrency.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSet>
#include <QtTest>

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

void TestConcurrency::test_concurrent_doc_numbers()
{
    const int threadCount = 8;
    const int numbersPerThread = 50;
    const QString connPrefix = "numConn";

    std::atomic<bool> startGate(false);
    QSet<QString> allNumbers;
    std::mutex setMutex;

    std::vector<std::thread> threads;
    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([this, i, &startGate, &allNumbers, &setMutex, &connPrefix]() {
            QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL", connPrefix + QString::number(i));
            if (!openConnection(db, m_testDbName, m_env, m_dbConfig, nullptr)) {
                QSqlDatabase::removeDatabase(connPrefix + QString::number(i));
                return;
            }
            while (!startGate.load())
                std::this_thread::yield();

            for (int k = 0; k < numbersPerThread; ++k) {
                QSqlQuery q(db);
                q.prepare("SELECT generate_doc_number(:t)");
                q.bindValue(":t", "receipt");
                if (q.exec() && q.next()) {
                    std::lock_guard<std::mutex> lk(setMutex);
                    allNumbers.insert(q.value(0).toString());
                }
            }

            db.close();
            QSqlDatabase::removeDatabase(connPrefix + QString::number(i));
        });
    }

    startGate.store(true);
    for (auto& th : threads)
        th.join();

    QCOMPARE(allNumbers.size(), qsizetype(threadCount * numbersPerThread));
}

void TestConcurrency::test_concurrent_rate_limit()
{
    const QString username = "conclimit";
    execOnTest("DELETE FROM tbl_users WHERE username = '" + username + "'");
    QVERIFY2(seedUser(username) != -1, qPrintable(m_lastError));

    const int threadCount = 20;
    std::atomic<bool> startGate(false);
    std::vector<RateLimitResult> results(threadCount);

    std::vector<std::thread> threads;
    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([&, i]() {
            rateLimitWorker("rateConn" + QString::number(i), m_testDbName, m_env, m_dbConfig, &startGate, &results[i],
                            username);
        });
    }

    startGate.store(true);
    for (auto& th : threads)
        th.join();

    int totalReturned = 0;
    int maxAttempts = 0;
    for (const RateLimitResult& r : results) {
        totalReturned += r.rowsReturned;
        maxAttempts = qMax(maxAttempts, r.maxAttempts);
    }

    // Ровно 5 попыток пробивают до активации блокировки, остальные отклонены WHERE-условием
    QCOMPARE(totalReturned, 5);
    QCOMPARE(maxAttempts, 5);

    QCOMPARE(countOnTest("SELECT failed_login_attempts FROM tbl_users WHERE username = '" + username + "'"), 5);
    QCOMPARE(
        countOnTest("SELECT count(*) FROM tbl_users WHERE username = '" + username + "' AND locked_until IS NOT NULL"),
        1);

    execOnTest("UPDATE tbl_users SET failed_login_attempts = 0, locked_until = NULL "
               "WHERE username = '" +
               username + "'");
}

void TestConcurrency::test_concurrent_migrations()
{
    QString migrationsDir;
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidates = {appDir + "/sql/migrations/", appDir + "/../sql/migrations/",
                              appDir + "/../../sql/migrations/"};
    for (const QString& c : candidates) {
        QDir d(c);
        if (d.exists()) {
            migrationsDir = d.absolutePath();
            break;
        }
    }
    QVERIFY2(!migrationsDir.isEmpty(), "Директория миграций не найдена");

    // Имитация одновременного старта: два последних файла снова «не применены»
    QSqlQuery del(m_testDb);
    QVERIFY2(del.exec("DELETE FROM schema_migrations "
                      "WHERE version IN ('006_terminal_status_check.sql', "
                      "                   '007_data_change_notify.sql')"),
             qPrintable(del.lastError().text()));

    std::atomic<bool> startGate(false);
    std::atomic<int> okCount(0);

    std::thread t1(
        [&]() { migrationWorker("migConn1", m_testDbName, m_env, m_dbConfig, &startGate, migrationsDir, &okCount); });
    std::thread t2(
        [&]() { migrationWorker("migConn2", m_testDbName, m_env, m_dbConfig, &startGate, migrationsDir, &okCount); });

    startGate.store(true);
    t1.join();
    t2.join();

    QCOMPARE(okCount.load(), 2);
    QCOMPARE(countOnTest("SELECT count(*) FROM schema_migrations"), 14);

    // Триггер уведомлений на месте и функция определена
    QCOMPARE(countOnTest("SELECT count(*) FROM pg_trigger "
                         "WHERE tgname = 'trg_data_change_notify_tblterminals'"),
             1);
    QCOMPARE(countOnTest("SELECT count(*) FROM pg_proc WHERE proname = 'notify_data_changed'"), 1);
}