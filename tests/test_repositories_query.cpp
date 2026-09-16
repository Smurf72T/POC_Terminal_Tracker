#include "test_repositories.h"

#include "database/repositories/caserepository.h"
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

    // Китованная SIM (status=1) закреплена за свободным терминалом —
    // состояние после документа «Установка SIM». Такая SIM занята и НЕ должна
    // считаться свободной на дашборде.
    QSqlQuery q(m_db);
    q.exec("INSERT INTO tblsimcards (simcardid, simnumber, status, notes) "
           "VALUES (5, '890100000000005', 1, 'установлена в свободный терминал')");
    q.exec("UPDATE tblterminals SET currentsimcardid2 = 5 WHERE terminalid = 1");

    // Свободны только SIM со status=0: SIM1, SIM3, SIM4. SIM2 — в аренде,
    // SIM5 — установлена в свободный терминал (китованная) — занята.
    QCOMPARE(repo.countAll(), 5);
    QCOMPARE(repo.countFree(), 3);

    // Отчёт о свободных SIM: статус 0 И не привязанные ни к одному терминалу.
    // Привязанные SIM1 (к SN-0001) и SIM3 (к SN-0004) исключены, SIM5 — статус 1.
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

void TestRepositories::caseOperations()
{
    CaseRepository repo(m_db);

    // Поступление: «Slim» × 3 и «Classic» × 2 → 5 единиц на складе (status 0).
    QVERIFY(repo.createBatch(QStringLiteral("Slim"), 3));
    QVERIFY(repo.createBatch(QStringLiteral("Classic"), 2));
    QCOMPARE(repo.countByStatus(0), 5);
    QCOMPARE(repo.countByStatus(1), 0);
    QCOMPARE(repo.countByStatus(2), 0);

    // Свободные остатки по типам.
    const auto free = repo.summarizeFree();
    QCOMPARE(free.size(), 2);
    QCOMPARE(free.at(0).first, QStringLiteral("Classic"));
    QCOMPARE(free.at(0).second, 2);
    QCOMPARE(free.at(1).first, QStringLiteral("Slim"));
    QCOMPARE(free.at(1).second, 3);

    // Выбор свободных.
    const auto freeSelection = repo.loadFreeForSelection();
    QCOMPARE(freeSelection.size(), 5);
    QCOMPARE(freeSelection.at(0).caseType, QStringLiteral("Classic"));

    // Первый свободный чехол и его загрузка по id.
    const int firstId = freeSelection.at(0).id;
    const models::CaseItem c1 = repo.loadById(firstId);
    QCOMPARE(c1.caseType, QStringLiteral("Classic"));
    QCOMPARE(c1.status, 0);

    // Установка на терминал 1 (свободный) — имитация lock(): status 0→1 + привязки.
    {
        QSqlQuery up(m_db);
        up.prepare("UPDATE tblcases SET status = 1, terminalid = 1 WHERE caseid = :id");
        up.bindValue(":id", firstId);
        QVERIFY(up.exec());
        QSqlQuery tp(m_db);
        tp.prepare("UPDATE tblterminals SET currentcaseid = :cid WHERE terminalid = 1");
        tp.bindValue(":cid", firstId);
        QVERIFY(tp.exec());
    }

    // Чехол больше не числится свободным; терминал 1 видит его как свой.
    QCOMPARE(repo.countByStatus(0), 4);
    QCOMPARE(repo.countByStatus(1), 1);
    QCOMPARE(repo.loadById(firstId).terminalId, 1);
    QCOMPARE(repo.loadByTerminal(1).id, firstId);
    QVERIFY(repo.loadByTerminal(999).id == 0);

    // Освобождение (имитация free()) — возврат на склад.
    {
        QSqlQuery up(m_db);
        up.prepare("UPDATE tblcases SET status = 0, terminalid = NULL WHERE caseid = :id");
        up.bindValue(":id", firstId);
        QVERIFY(up.exec());
        QSqlQuery tp(m_db);
        tp.prepare("UPDATE tblterminals SET currentcaseid = NULL WHERE terminalid = 1");
        QVERIFY(tp.exec());
    }
    QCOMPARE(repo.countByStatus(0), 5);
    QCOMPARE(repo.loadById(firstId).terminalId, 0);

    // Строки документов: поступление с типом и количеством.
    {
        QSqlQuery dh(m_db);
        dh.exec("INSERT INTO tblcaseincomedocs (caseincomedocid, docnumber, docdate, comments) "
                "VALUES (1, 'ПЧ-00001', '2026-09-01', '')");
        QSqlQuery dd(m_db);
        dd.exec("INSERT INTO tblcaseincomedetails (caseincomedetailid, caseincomedocid, casetype, qty) "
                "VALUES (1, 1, 'Slim', 3), (2, 1, 'Classic', 2)");
        const auto rows = repo.loadIncomeRows(1);
        QCOMPARE(rows.size(), 2);
        QCOMPARE(rows.at(0).caseType, QStringLiteral("Slim"));
        QCOMPARE(rows.at(0).qty, 3);
        QCOMPARE(repo.loadIncomeHeader(1).docNumber, QStringLiteral("ПЧ-00001"));
        QVERIFY(repo.deleteIncomeDetails(1));
        QVERIFY(repo.deleteIncomeHeader(1));
        QVERIFY(repo.loadIncomeRows(1).isEmpty());
    }

    // Строки установки и списания (пустые и с данными).
    {
        QSqlQuery ih(m_db);
        ih.exec("INSERT INTO tblcaseinstalldocs (caseinstalldocid, docnumber, docdate, comments) "
                "VALUES (1, 'УЧ-00001', '2026-09-02', '')");
        QSqlQuery id(m_db);
        id.exec("INSERT INTO tblcaseinstalldetails (caseinstalldetailid, caseinstalldocid, terminalid, caseid) "
                "VALUES (1, 1, 2, 999)");
        const auto rows = repo.loadInstallRows(1);
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.at(0).terminalSerial, QStringLiteral("SN-0002"));
        QVERIFY(repo.deleteInstallDetails(1));
        QVERIFY(repo.deleteInstallHeader(1));

        QSqlQuery wh(m_db);
        wh.exec("INSERT INTO tblcasewriteoffdocs (casewriteoffdocid, docnumber, docdate, comments) "
                "VALUES (1, 'СЧ-00001', '2026-09-03', '')");
        QSqlQuery wd(m_db);
        wd.exec("INSERT INTO tblcasewriteoffdetails (casewriteoffdetailid, casewriteoffdocid, caseid, reason) "
                "VALUES (1, 1, 2, 'брак')");
        const auto wrows = repo.loadWriteoffRows(1);
        QCOMPARE(wrows.size(), 1);
        QCOMPARE(wrows.at(0).reason, QStringLiteral("брак"));
        QVERIFY(repo.deleteWriteoffDetails(1));
        QVERIFY(repo.deleteWriteoffHeader(1));
    }
}