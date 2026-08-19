#include "test_concurrency.h"

#include <QSqlError>
#include <QtTest>

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

void TestConcurrency::test_concurrent_sim_assignment()
{
    const int threadCount = 8;
    int terminalId = seedTerminal("КОНК-СИМ-ТЕРМ-001", 0);
    QVERIFY2(terminalId != -1, qPrintable(m_lastError));
    int simId = seedSim("КОНК-СИМ-001");
    QVERIFY2(simId != -1, qPrintable(m_lastError));

    std::atomic<bool> startGate(false);
    std::atomic<int> successCount(0);
    std::mutex errorMutex;
    QString firstError;

    std::vector<std::thread> threads;
    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([&, i]() {
            rentWorker("simConn" + QString::number(i), m_testDbName, m_env, m_dbConfig, &startGate, &successCount,
                       &errorMutex, &firstError, terminalId, simId);
        });
    }

    startGate.store(true);
    for (auto& th : threads)
        th.join();

    QCOMPARE(successCount.load(), 1);
    QVERIFY2(!firstError.isEmpty(), qPrintable("ожидались конкурентные отказы"));

    QCOMPARE(countOnTest("SELECT status FROM tblterminals WHERE terminalid = " + QString::number(terminalId)), 1);
    QCOMPARE(countOnTest("SELECT currentsimcardid FROM tblterminals WHERE terminalid = " + QString::number(terminalId)),
             simId);
    QCOMPARE(countOnTest("SELECT status FROM tblsimcards WHERE simcardid = " + QString::number(simId)), 1);

    resetTerminal(terminalId, simId);
}

void TestConcurrency::test_concurrent_rental_load()
{
    const int threadCount = 8;
    std::vector<int> terminalIds;
    std::vector<int> simIds;
    for (int i = 0; i < threadCount; ++i) {
        int tid = seedTerminal("КОНК-НАГРУЗКА-" + QString::number(i), 0);
        QVERIFY2(tid != -1, qPrintable(m_lastError));
        int sid = seedSim("КОНК-SIM-НАГР-" + QString::number(i));
        QVERIFY2(sid != -1, qPrintable(m_lastError));
        terminalIds.push_back(tid);
        simIds.push_back(sid);
    }

    std::atomic<bool> startGate(false);
    std::atomic<int> successCount(0);
    std::mutex errorMutex;
    QString firstError;

    std::vector<std::thread> threads;
    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([&, i]() {
            rentWorker("loadConn" + QString::number(i), m_testDbName, m_env, m_dbConfig, &startGate, &successCount,
                       &errorMutex, &firstError, terminalIds[i], simIds[i]);
        });
    }

    startGate.store(true);
    for (auto& th : threads)
        th.join();

    QCOMPARE(successCount.load(), threadCount);
    QVERIFY2(firstError.isEmpty(), qPrintable("неожиданная ошибка: " + firstError));

    QCOMPARE(countOnTest("SELECT count(*) FROM tblterminals WHERE status = 1"), threadCount);
    QCOMPARE(countOnTest("SELECT count(*) FROM tblsimcards WHERE status = 1"), threadCount);
    QCOMPARE(countOnTest("SELECT count(DISTINCT currentsimcardid) FROM tblterminals "
                         "WHERE currentsimcardid IS NOT NULL"),
             threadCount);
}

void TestConcurrency::test_concurrent_batch_vs_rental()
{
    for (int round = 0; round < 4; ++round) {
        int terminalId = seedTerminal("КОНК-ГОНКА-" + QString::number(round), 0);
        QVERIFY2(terminalId != -1, qPrintable(m_lastError));
        int simId = seedSim("КОНК-SIM-ГОНКА-" + QString::number(round));
        QVERIFY2(simId != -1, qPrintable(m_lastError));

        std::atomic<bool> startGate(false);
        std::atomic<int> rentSuccess(0);
        std::atomic<int> batchSuccess(0);
        std::mutex errorMutex;
        QString firstError;

        std::thread t1([&]() {
            rentWorker("raceRentConn", m_testDbName, m_env, m_dbConfig, &startGate, &rentSuccess, &errorMutex,
                       &firstError, terminalId, simId);
        });
        std::thread t2([&]() {
            batchWorker("raceBatchConn", m_testDbName, m_env, m_dbConfig, &startGate, &batchSuccess, terminalId, 0, 1);
        });

        startGate.store(true);
        t1.join();
        t2.join();

        QCOMPARE(rentSuccess.load() + batchSuccess.load(), 1);

        // Согласованность финального состояния: терминал всегда «в аренде» (статус 1),
        // при этом либо аренда (SIM привязана и занята), либо массовая смена (SIM свободна)
        QCOMPARE(countOnTest("SELECT status FROM tblterminals WHERE terminalid = " + QString::number(terminalId)), 1);
        int simStatus = countOnTest("SELECT status FROM tblsimcards WHERE simcardid = " + QString::number(simId));
        int currentSim =
            countOnTest("SELECT currentsimcardid FROM tblterminals WHERE terminalid = " + QString::number(terminalId));
        if (rentSuccess.load() == 1) {
            QCOMPARE(currentSim, simId);
            QCOMPARE(simStatus, 1);
        } else {
            QCOMPARE(currentSim, 0);
            QCOMPARE(simStatus, 0);
        }
    }
}