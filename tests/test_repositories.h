#ifndef TEST_REPOSITORIES_H
#define TEST_REPOSITORIES_H

#include <QObject>
#include <QSqlDatabase>
#include <QString>

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
    void terminalLoadMethods();
    void terminalUpdateMethod();
    void simCardLoadMethods();
    void clientLoadMethods();
    void documentModelMethods();
    void receiptItemQueries();

private:
    static void insertTerminal(int id, const QString& serial, int status, int modelId, int simId);
    static void insertTerminalFull(int id, const QString& serial, int status, int modelId, int simId,
                                   const QString& imei1, const QString& imei2, int deactivated);
    static void insertModel(int id, const QString& name);
    static void insertSim(int id, const QString& number, int status, const QString& notes);
    static void insertClient(int id, const QString& name);
    static void insertClientFull(int id, const QString& name, const QString& inn, const QString& address,
                                 const QString& phone, const QString& email);
    static void insertRentalDoc(int id, int clientId, const QString& docNumber, const QString& docDate);
    static void insertRentalDetail(int id, int rentalDocId, int terminalId, int simId);
    static void insertReceiptDoc(int id, const QString& docNumber, const QString& docDate);
    static void insertReceiptDetail(int id, int receiptDocId, int terminalId);
    static void insertReceiptItem(int id, int docId, int modelId, int qty);
    static void insertReceiptSerial(int id, int itemId, int linenum, const QString& serial, const QString& imei1,
                                    const QString& imei2);
    static void insertReturnDoc(int id, int clientId, const QString& docNumber, const QString& docDate);
    static void insertReturnDetail(int id, int returnDocId, int terminalId);
    static void insertPayment(int id, int year, int month, double amount);

    QSqlDatabase m_db;
};

#endif // TEST_REPOSITORIES_H