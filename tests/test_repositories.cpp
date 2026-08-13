#include <QtTest/QtTest>

#include "database/repositories/clientrepository.h"
#include "database/repositories/documentrepository.h"
#include "database/repositories/paymentrepository.h"
#include "database/repositories/simcardrepository.h"
#include "database/repositories/terminalrepository.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlQueryModel>

// Тесты слоя репозиториев на изолированной SQLite-БД в памяти.
// Схема намеренно минимальна — проверяются только колонки, используемые
// запросами репозиториев.
class TestRepositories : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void terminalQueries();
    void simCardQueries();
    void clientQueries();
    void documentQueries();
    void paymentQueries();
    void populateMethodsFillModels();

private:
    static void insertTerminal(int id, const QString& serial, int status, int modelId, int simId);
    static void insertModel(int id, const QString& name);
    static void insertSim(int id, const QString& number, int status, const QString& notes);
    static void insertClient(int id, const QString& name);
    static void insertRentalDoc(int id, int clientId, const QString& docNumber, const QString& docDate);
    static void insertRentalDetail(int id, int rentalDocId, int terminalId, int simId);
    static void insertPayment(int id, int year, int month, double amount);

    QSqlDatabase m_db;
};

void TestRepositories::initTestCase()
{
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(":memory:");
    if (!m_db.open())
        QFAIL("Cannot open in-memory SQLite database");

    QSqlQuery q(m_db);
    q.exec("CREATE TABLE tblmodels (modelid INTEGER PRIMARY KEY, modelname TEXT)");
    q.exec("CREATE TABLE tblsimcards (simcardid INTEGER PRIMARY KEY, simnumber TEXT, status INTEGER, notes TEXT)");
    q.exec("CREATE TABLE tblterminals (terminalid INTEGER PRIMARY KEY, serialnumber TEXT, status INTEGER, "
           "modelid INTEGER, currentsimcardid INTEGER)");
    q.exec("CREATE TABLE tblclients (clientid INTEGER PRIMARY KEY, clientname TEXT)");
    q.exec("CREATE TABLE tblrentaldocs (rentaldocid INTEGER PRIMARY KEY, clientid INTEGER, docnumber TEXT, "
           "docdate TEXT)");
    q.exec("CREATE TABLE tblrentaldetails (rentaldetailid INTEGER PRIMARY KEY, rentaldocid INTEGER, "
           "terminalid INTEGER, simcardid INTEGER)");
    q.exec("CREATE TABLE tblpayments (paymentid INTEGER PRIMARY KEY, periodyear INTEGER, periodmonth INTEGER, "
           "amount REAL)");
    q.exec("CREATE TABLE tblreceiptdocs (receiptdocid INTEGER PRIMARY KEY, docnumber TEXT, docdate TEXT)");
    q.exec("CREATE TABLE tblreturndocs (returndocid INTEGER PRIMARY KEY, docnumber TEXT, docdate TEXT)");
    q.exec("CREATE TABLE tblstatuschangedocs (statuschangedocid INTEGER PRIMARY KEY, docnumber TEXT, docdate TEXT)");

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

void TestRepositories::insertTerminal(int id, const QString& serial, int status, int modelId, int simId)
{
    QSqlQuery q(QSqlDatabase::database(QLatin1String()));
    q.prepare("INSERT INTO tblterminals (terminalid, serialnumber, status, modelid, currentsimcardid) "
              "VALUES (?, ?, ?, ?, ?)");
    q.addBindValue(id);
    q.addBindValue(serial);
    q.addBindValue(status);
    q.addBindValue(modelId);
    q.addBindValue(simId);
    q.exec();
}

void TestRepositories::insertModel(int id, const QString& name)
{
    QSqlQuery q(QSqlDatabase::database(QLatin1String()));
    q.prepare("INSERT INTO tblmodels (modelid, modelname) VALUES (?, ?)");
    q.addBindValue(id);
    q.addBindValue(name);
    q.exec();
}

void TestRepositories::insertSim(int id, const QString& number, int status, const QString& notes)
{
    QSqlQuery q(QSqlDatabase::database(QLatin1String()));
    q.prepare("INSERT INTO tblsimcards (simcardid, simnumber, status, notes) VALUES (?, ?, ?, ?)");
    q.addBindValue(id);
    q.addBindValue(number);
    q.addBindValue(status);
    q.addBindValue(notes);
    q.exec();
}

void TestRepositories::insertClient(int id, const QString& name)
{
    QSqlQuery q(QSqlDatabase::database(QLatin1String()));
    q.prepare("INSERT INTO tblclients (clientid, clientname) VALUES (?, ?)");
    q.addBindValue(id);
    q.addBindValue(name);
    q.exec();
}

void TestRepositories::insertRentalDoc(int id, int clientId, const QString& docNumber, const QString& docDate)
{
    QSqlQuery q(QSqlDatabase::database(QLatin1String()));
    q.prepare("INSERT INTO tblrentaldocs (rentaldocid, clientid, docnumber, docdate) VALUES (?, ?, ?, ?)");
    q.addBindValue(id);
    q.addBindValue(clientId);
    q.addBindValue(docNumber);
    q.addBindValue(docDate);
    q.exec();
}

void TestRepositories::insertRentalDetail(int id, int rentalDocId, int terminalId, int simId)
{
    QSqlQuery q(QSqlDatabase::database(QLatin1String()));
    q.prepare("INSERT INTO tblrentaldetails (rentaldetailid, rentaldocid, terminalid, simcardid) "
              "VALUES (?, ?, ?, ?)");
    q.addBindValue(id);
    q.addBindValue(rentalDocId);
    q.addBindValue(terminalId);
    q.addBindValue(simId);
    q.exec();
}

void TestRepositories::insertPayment(int id, int year, int month, double amount)
{
    QSqlQuery q(QSqlDatabase::database(QLatin1String()));
    q.prepare("INSERT INTO tblpayments (paymentid, periodyear, periodmonth, amount) VALUES (?, ?, ?, ?)");
    q.addBindValue(id);
    q.addBindValue(year);
    q.addBindValue(month);
    q.addBindValue(amount);
    q.exec();
}

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

void TestRepositories::populateMethodsFillModels()
{
    TerminalRepository terminals(m_db);
    auto* termModel = new QSqlQueryModel(this);
    terminals.populateFreeTerminals(termModel);
    QCOMPARE(termModel->rowCount(), 2);
    QCOMPARE(termModel->columnCount(), 3);
    QCOMPARE(termModel->data(termModel->index(0, 0)).toString(), QString("SN-0001"));
    QCOMPARE(termModel->data(termModel->index(0, 1)).toString(), QString("PAX-A920"));
    QCOMPARE(termModel->data(termModel->index(1, 0)).toString(), QString("SN-0004"));

    SimCardRepository sims(m_db);
    auto* simModel = new QSqlQueryModel(this);
    sims.populateFreeSimCards(simModel);
    // Только SIM4: статус 0 и не привязана ни к одному терминалу.
    QCOMPARE(simModel->rowCount(), 1);
    QCOMPARE(simModel->data(simModel->index(0, 0)).toString(), QString("890100000000004"));

    ClientRepository clients(m_db);
    auto* rentalModel = new QSqlQueryModel(this);
    clients.populateRentedTerminals(rentalModel, 1);
    QCOMPARE(rentalModel->rowCount(), 2);
    QCOMPARE(rentalModel->data(rentalModel->index(0, 1)).toString(), QString("SN-0002"));
    QCOMPARE(rentalModel->data(rentalModel->index(1, 1)).toString(), QString("SN-0003"));
}

QTEST_MAIN(TestRepositories)
#include "test_repositories.moc"