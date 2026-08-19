#include "test_repositories.h"

#include <QDate>
#include <QSqlError>
#include <QSqlQuery>
#include <QtTest/QtTest>

void TestRepositories::initTestCase()
{
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(":memory:");
    if (!m_db.open())
        QFAIL("Cannot open in-memory SQLite database");

    QSqlQuery q(m_db);
    q.exec("CREATE TABLE tblmodels (modelid INTEGER PRIMARY KEY, modelname TEXT)");
    q.exec("CREATE TABLE tblsimcards (simcardid INTEGER PRIMARY KEY, simnumber TEXT, status INTEGER, notes TEXT, "
           "createdat TEXT)");
    q.exec("CREATE TABLE tblterminals (terminalid INTEGER PRIMARY KEY, serialnumber TEXT, status INTEGER, "
           "modelid INTEGER, currentsimcardid INTEGER, currentsimcardid2 INTEGER, imei1 TEXT, imei2 TEXT, "
           "is_deactivated INTEGER NOT NULL DEFAULT 0, purchasedate TEXT, notes TEXT, "
           "was_repaired INTEGER NOT NULL DEFAULT 0)");
    q.exec("CREATE TABLE tblclients (clientid INTEGER PRIMARY KEY, clientname TEXT, inn TEXT, address TEXT, "
           "contactphone TEXT, contactemail TEXT)");
    q.exec("CREATE TABLE tblrentaldocs (rentaldocid INTEGER PRIMARY KEY, clientid INTEGER, docnumber TEXT, "
           "docdate TEXT, comments TEXT)");
    q.exec("CREATE TABLE tblrentaldetails (rentaldetailid INTEGER PRIMARY KEY, rentaldocid INTEGER, "
           "terminalid INTEGER, simcardid INTEGER, simcardid2 INTEGER, comment TEXT)");
    q.exec("CREATE TABLE tblpayments (paymentid INTEGER PRIMARY KEY, periodyear INTEGER, periodmonth INTEGER, "
           "amount REAL)");
    q.exec("CREATE TABLE tblreceiptdocs (receiptdocid INTEGER PRIMARY KEY, docnumber TEXT, docdate TEXT, "
           "comments TEXT)");
    q.exec("CREATE TABLE tblreceiptdetails (receiptdetailid INTEGER PRIMARY KEY, receiptdocid INTEGER, "
           "terminalid INTEGER)");
    q.exec("CREATE TABLE tblreceiptitems (receiptitemid INTEGER PRIMARY KEY, receiptdocid INTEGER, "
           "modelid INTEGER, qty INTEGER)");
    q.exec("CREATE TABLE tblreceiptserials (receiptserialid INTEGER PRIMARY KEY, receiptitemid INTEGER, "
           "linenum INTEGER, serialnumber TEXT, imei1 TEXT, imei2 TEXT)");
    q.exec("CREATE TABLE tblreturndocs (returndocid INTEGER PRIMARY KEY, docnumber TEXT, docdate TEXT, "
           "clientid INTEGER, comments TEXT)");
    q.exec("CREATE TABLE tblreturndetails (returndetailid INTEGER PRIMARY KEY, returndocid INTEGER, "
           "terminalid INTEGER)");
    q.exec("CREATE TABLE tblstatuschangedocs (statuschangedocid INTEGER PRIMARY KEY, docnumber TEXT, docdate TEXT, "
           "comment TEXT)");

    insertModel(1, "PAX-A920");
    insertModel(2, "PAX-A910");
    insertSim(1, "890100000000001", 0, QString());
    insertSim(2, "890100000000002", 1, "в работе");
    insertSim(3, "890100000000003", 0, "привязана к свободному терминалу");
    insertSim(4, "890100000000004", 0, "свободна");

    // 0 — свободен, 1 — в аренде
    insertTerminal(1, "SN-0001", 0, 1, 1);
    insertTerminal(2, "SN-0002", 1, 1, 2);
    insertTerminal(3, "SN-0003", 1, 2, 0);
    // SIM 3 закреплена за свободным терминалом — считается свободной.
    insertTerminal(4, "SN-0004", 0, 2, 3);

    insertClient(1, "ООО «Альфа»");
    insertClient(2, "ИП Иванов");

    // Клиент 1 арендует терминалы 2 и 3 (status=1). Клиент 2 арендовал
    // терминал 1, но он уже возвращён (status=0) — в отчёте не учитывается.
    insertRentalDoc(1, 1, "AR-2026-00001", "2026-07-01");
    insertRentalDetail(1, 1, 2, 2);
    insertRentalDetail(2, 1, 3, 0);
    insertRentalDoc(2, 2, "AR-2026-00002", "2026-08-01");
    insertRentalDetail(3, 2, 1, 1);

    // Оплаты за последние месяцы — относительно текущей даты, чтобы тест
    // не зависел от календарного месяца запуска.
    const QDate now = QDate::currentDate();
    insertPayment(1, now.addMonths(-2).year(), now.addMonths(-2).month(), 1500.0);
    insertPayment(2, now.addMonths(-1).year(), now.addMonths(-1).month(), 2000.0);
    insertPayment(3, now.addMonths(-1).year(), now.addMonths(-1).month(), 500.0);
    insertPayment(4, now.year(), now.month(), 1000.0);
}

QTEST_MAIN(TestRepositories)
#include "test_repositories.moc"