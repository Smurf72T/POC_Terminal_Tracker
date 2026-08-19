#include "test_repositories.h"

#include "database/repositories/clientrepository.h"
#include "database/repositories/documentrepository.h"
#include "database/repositories/paymentrepository.h"
#include "database/repositories/simcardrepository.h"
#include "database/repositories/terminalrepository.h"

#include <QDate>
#include <QHash>
#include <QSqlError>
#include <QSqlQuery>
#include <QtTest/QtTest>

void TestRepositories::terminalQueries()
{
    TerminalRepository repo(m_db);

    QCOMPARE(repo.countAll(), 4);
    QCOMPARE(repo.countByStatus(0), 2);
    QCOMPARE(repo.countByStatus(1), 2);
    QCOMPARE(repo.countByStatus(2), 0);

    const auto counts = repo.statusCounts();
    QCOMPARE(counts.size(), 2);
    for (const auto& c : counts) {
        if (c.status == 0)
            QCOMPARE(c.count, 2);
        else if (c.status == 1)
            QCOMPARE(c.count, 2);
        else
            QFAIL("Unexpected status group");
    }

    const auto free = repo.loadFreeTerminals();
    QCOMPARE(free.size(), 2);
    QCOMPARE(free.at(0).serialNumber, QString("SN-0001"));
    QCOMPARE(free.at(0).simStatus, QString("890100000000001"));

    QCOMPARE(repo.findIdBySerial("SN-0002"), 2);
    QCOMPARE(repo.findIdBySerial("SN-UNKNOWN"), -1);

    const auto ids = repo.loadSerialsWithIds();
    QCOMPARE(ids.size(), 4);
    QCOMPARE(ids.first().first, QString("SN-0001"));
    QCOMPARE(ids.first().second, 1);
}

void TestRepositories::simCardQueries()
{
    SimCardRepository repo(m_db);
    QCOMPARE(repo.countAll(), 4);
    // Свободны: SIM1, SIM3 (статус 0) и SIM4 (статус 0). SIM2 — в работе.
    QCOMPARE(repo.countFree(), 3);

    // Отчёт о свободных SIM: статус 0 И не привязанные ни к одному терминалу.
    // Привязанные SIM1 (к SN-0001) и SIM3 (к SN-0004) исключены.
    const auto free = repo.loadFreeSimCards();
    QCOMPARE(free.size(), 1);
    QCOMPARE(free.at(0).simNumber, QString("890100000000004"));
}

void TestRepositories::clientQueries()
{
    ClientRepository repo(m_db);
    QCOMPARE(repo.countAll(), 2);

    // Клиент 1 арендует терминалы 2 и 3 (status=1). Клиент 2 арендовал
    // терминал 1, но он возвращён — из выборки (INNER JOIN по status=1) выпадает.
    const auto stats = repo.loadRentalStatistics();
    QCOMPARE(stats.size(), 1);
    QCOMPARE(stats.at(0).clientId, 1);
    QCOMPARE(stats.at(0).activeTerminals, 2);

    const auto terminals = repo.loadRentedTerminals(1);
    QCOMPARE(terminals.size(), 2);
    QCOMPARE(terminals.at(0).serialNumber, QString("SN-0002"));
    QCOMPARE(terminals.at(1).serialNumber, QString("SN-0003"));

    QCOMPARE(repo.loadRentedTerminals(2).size(), 0);
}

void TestRepositories::documentQueries()
{
    DocumentRepository repo(m_db);

    QSqlQuery q(m_db);
    q.exec("INSERT INTO tblreceiptdocs (receiptdocid, docnumber, docdate) VALUES (1, 'PR-2026-00001', '2026-08-02')");
    q.exec("INSERT INTO tblreturndocs (returndocid, docnumber, docdate) VALUES (1, 'RT-2026-00001', '2026-06-01')");
    q.exec("INSERT INTO tblstatuschangedocs (statuschangedocid, docnumber, docdate) "
           "VALUES (1, 'SC-2026-00001', '2026-05-01')");

    const auto docs = repo.recentDocuments();
    // 2 аренды + поступление + возврат + изменение статуса, сортировка по дате DESC.
    QCOMPARE(docs.size(), 5);
    QCOMPARE(docs.at(0).docType, DocumentRepository::Receipt);
    QCOMPARE(docs.at(0).date, QString("2026-08-02"));
    QCOMPARE(docs.at(1).docType, DocumentRepository::Rental);
    QCOMPARE(docs.at(1).number, QString("AR-2026-00002"));
}

void TestRepositories::receiptItemQueries()
{
    DocumentRepository repo(m_db);

    insertReceiptDoc(3, "PR-2026-00002", "2026-08-10");
    insertReceiptItem(1, 3, 1, 2);
    insertReceiptSerial(1, 1, 1, "SN-1001", "111111111111111", QString());
    insertReceiptSerial(2, 1, 2, "SN-1002", "222222222222222", "333333333333333");

    const auto items = repo.loadReceiptItems(3);
    QCOMPARE(items.size(), 1);
    QCOMPARE(items.at(0).modelId, 1);
    QCOMPARE(items.at(0).modelName, QString("PAX-A920"));
    QCOMPARE(items.at(0).qty, 2);
    QCOMPARE(items.at(0).serials.size(), 2);
    QCOMPARE(items.at(0).serials.at(0).serialNumber, QString("SN-1001"));
    QCOMPARE(items.at(0).serials.at(0).imei1, QString("111111111111111"));
    QCOMPARE(items.at(0).serials.at(1).serialNumber, QString("SN-1002"));
    QCOMPARE(items.at(0).serials.at(1).imei2, QString("333333333333333"));

    // Документ без строк «исходника» (легаси/свежий) и несуществующий.
    QVERIFY(repo.loadReceiptItems(2).isEmpty());
    QVERIFY(repo.loadReceiptItems(999).isEmpty());
}

void TestRepositories::paymentQueries()
{
    PaymentRepository repo(m_db);
    const auto revenue = repo.revenueByMonth(6);

    // Ровно 6 месяцев — недостающие заполнены нулями.
    QCOMPARE(revenue.size(), 6);

    const QDate now = QDate::currentDate();
    auto monthLabel = [](const QDate& d) {
        return QString("%1-%2").arg(d.year(), 4, 10, QLatin1Char('0')).arg(d.month(), 2, 10, QLatin1Char('0'));
    };

    QHash<QString, double> byMonth;
    for (const auto& r : revenue)
        byMonth.insert(r.month, r.total);

    QCOMPARE(byMonth.value(monthLabel(now)), 1000.0);
    QCOMPARE(byMonth.value(monthLabel(now.addMonths(-1))), 2500.0); // 2000 + 500
    QCOMPARE(byMonth.value(monthLabel(now.addMonths(-2))), 1500.0);
}