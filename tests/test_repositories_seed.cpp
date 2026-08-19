#include "test_repositories.h"

#include <QSqlQuery>

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

void TestRepositories::insertTerminalFull(int id, const QString& serial, int status, int modelId, int simId,
                                          const QString& imei1, const QString& imei2, int deactivated)
{
    QSqlQuery q(QSqlDatabase::database(QLatin1String()));
    q.prepare("INSERT INTO tblterminals (terminalid, serialnumber, status, modelid, currentsimcardid, "
              "imei1, imei2, is_deactivated) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(id);
    q.addBindValue(serial);
    q.addBindValue(status);
    q.addBindValue(modelId);
    q.addBindValue(simId);
    q.addBindValue(imei1);
    q.addBindValue(imei2);
    q.addBindValue(deactivated);
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

void TestRepositories::insertClientFull(int id, const QString& name, const QString& inn, const QString& address,
                                        const QString& phone, const QString& email)
{
    QSqlQuery q(QSqlDatabase::database(QLatin1String()));
    q.prepare("INSERT INTO tblclients (clientid, clientname, inn, address, contactphone, contactemail) "
              "VALUES (?, ?, ?, ?, ?, ?)");
    q.addBindValue(id);
    q.addBindValue(name);
    q.addBindValue(inn);
    q.addBindValue(address);
    q.addBindValue(phone);
    q.addBindValue(email);
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

void TestRepositories::insertReceiptDoc(int id, const QString& docNumber, const QString& docDate)
{
    QSqlQuery q(QSqlDatabase::database(QLatin1String()));
    q.prepare("INSERT INTO tblreceiptdocs (receiptdocid, docnumber, docdate, comments) VALUES (?, ?, ?, ?)");
    q.addBindValue(id);
    q.addBindValue(docNumber);
    q.addBindValue(docDate);
    q.addBindValue(QString());
    q.exec();
}

void TestRepositories::insertReceiptDetail(int id, int receiptDocId, int terminalId)
{
    QSqlQuery q(QSqlDatabase::database(QLatin1String()));
    q.prepare("INSERT INTO tblreceiptdetails (receiptdetailid, receiptdocid, terminalid) VALUES (?, ?, ?)");
    q.addBindValue(id);
    q.addBindValue(receiptDocId);
    q.addBindValue(terminalId);
    q.exec();
}

void TestRepositories::insertReceiptItem(int id, int docId, int modelId, int qty)
{
    QSqlQuery q(QSqlDatabase::database(QLatin1String()));
    q.prepare("INSERT INTO tblreceiptitems (receiptitemid, receiptdocid, modelid, qty) VALUES (?, ?, ?, ?)");
    q.addBindValue(id);
    q.addBindValue(docId);
    q.addBindValue(modelId);
    q.addBindValue(qty);
    q.exec();
}

void TestRepositories::insertReceiptSerial(int id, int itemId, int linenum, const QString& serial,
                                           const QString& imei1, const QString& imei2)
{
    QSqlQuery q(QSqlDatabase::database(QLatin1String()));
    q.prepare("INSERT INTO tblreceiptserials (receiptserialid, receiptitemid, linenum, serialnumber, imei1, imei2) "
              "VALUES (?, ?, ?, ?, ?, ?)");
    q.addBindValue(id);
    q.addBindValue(itemId);
    q.addBindValue(linenum);
    q.addBindValue(serial);
    q.addBindValue(imei1);
    q.addBindValue(imei2);
    q.exec();
}

void TestRepositories::insertReturnDoc(int id, int clientId, const QString& docNumber, const QString& docDate)
{
    QSqlQuery q(QSqlDatabase::database(QLatin1String()));
    q.prepare("INSERT INTO tblreturndocs (returndocid, docnumber, docdate, clientid, comments) "
              "VALUES (?, ?, ?, ?, ?)");
    q.addBindValue(id);
    q.addBindValue(docNumber);
    q.addBindValue(docDate);
    q.addBindValue(clientId);
    q.addBindValue(QString());
    q.exec();
}

void TestRepositories::insertReturnDetail(int id, int returnDocId, int terminalId)
{
    QSqlQuery q(QSqlDatabase::database(QLatin1String()));
    q.prepare("INSERT INTO tblreturndetails (returndetailid, returndocid, terminalid) VALUES (?, ?, ?)");
    q.addBindValue(id);
    q.addBindValue(returnDocId);
    q.addBindValue(terminalId);
    q.exec();
}