#include "test_db_integration.h"

#include <QSqlError>
#include <QtTest>

void TestDbIntegration::test_business_flow()
{
    setAppValue("app.username", "flow");
    setAppValue("app.role", "admin");

    QSqlQuery man(m_testDb);
    man.prepare("INSERT INTO tblmanufacturers (manufacturername) VALUES (:n) RETURNING manufacturerid");
    man.bindValue(":n", "Производитель-Тест");
    QVERIFY2(man.exec(), qPrintable(man.lastError().text()));
    QVERIFY(man.next());
    int manId = man.value(0).toInt();

    QSqlQuery model(m_testDb);
    model.prepare("INSERT INTO tblmodels (manufacturerid, modelname) VALUES (:m, :n) RETURNING modelid");
    model.bindValue(":m", manId);
    model.bindValue(":n", "Модель-Тест");
    QVERIFY2(model.exec(), qPrintable(model.lastError().text()));
    QVERIFY(model.next());
    int modelId = model.value(0).toInt();

    QSqlQuery sim(m_testDb);
    sim.prepare("INSERT INTO tblsimcards (simnumber) VALUES (:n) RETURNING simcardid");
    sim.bindValue(":n", "ТЕСТ-SIM-001");
    QVERIFY2(sim.exec(), qPrintable(sim.lastError().text()));
    QVERIFY(sim.next());
    int simId = sim.value(0).toInt();

    QSqlQuery client(m_testDb);
    client.prepare("INSERT INTO tblclients (clientname) VALUES (:n) RETURNING clientid");
    client.bindValue(":n", "Клиент-Тест");
    QVERIFY2(client.exec(), qPrintable(client.lastError().text()));
    QVERIFY(client.next());
    int clientId = client.value(0).toInt();

    QSqlQuery term(m_testDb);
    term.prepare("INSERT INTO tblterminals (serialnumber, modelid, status) "
                 "VALUES (:s, :m, 0) RETURNING terminalid");
    term.bindValue(":s", "ТЕРМ-ТЕСТ-001");
    term.bindValue(":m", modelId);
    QVERIFY2(term.exec(), qPrintable(term.lastError().text()));
    QVERIFY(term.next());
    int termId = term.value(0).toInt();

    QString rNum = generateNumber("receipt");
    QVERIFY2(rNum.startsWith("ПП-"), qPrintable(rNum));
    QSqlQuery receipt(m_testDb);
    receipt.prepare("INSERT INTO tblreceiptdocs (docnumber) VALUES (:n) RETURNING receiptdocid");
    receipt.bindValue(":n", rNum);
    QVERIFY2(receipt.exec(), qPrintable(receipt.lastError().text()));
    QVERIFY(receipt.next());
    int receiptId = receipt.value(0).toInt();

    QSqlQuery recDetail(m_testDb);
    recDetail.prepare("INSERT INTO tblreceiptdetails (receiptdocid, terminalid) VALUES (:d, :t)");
    recDetail.bindValue(":d", receiptId);
    recDetail.bindValue(":t", termId);
    QVERIFY2(recDetail.exec(), qPrintable(recDetail.lastError().text()));

    QString rentNum = generateNumber("rental");
    QVERIFY2(rentNum.startsWith("АР-"), qPrintable(rentNum));
    QSqlQuery rental(m_testDb);
    rental.prepare("INSERT INTO tblrentaldocs (docnumber, clientid) VALUES (:n, :c) RETURNING rentaldocid");
    rental.bindValue(":n", rentNum);
    rental.bindValue(":c", clientId);
    QVERIFY2(rental.exec(), qPrintable(rental.lastError().text()));
    QVERIFY(rental.next());
    int rentalId = rental.value(0).toInt();

    QSqlQuery rentDetail(m_testDb);
    rentDetail.prepare("INSERT INTO tblrentaldetails (rentaldocid, terminalid, simcardid) "
                       "VALUES (:d, :t, :s)");
    rentDetail.bindValue(":d", rentalId);
    rentDetail.bindValue(":t", termId);
    rentDetail.bindValue(":s", simId);
    QVERIFY2(rentDetail.exec(), qPrintable(rentDetail.lastError().text()));

    QSqlQuery rentTerm(m_testDb);
    rentTerm.prepare("UPDATE tblterminals SET status = 1, currentsimcardid = :s WHERE terminalid = :t");
    rentTerm.bindValue(":s", simId);
    rentTerm.bindValue(":t", termId);
    QVERIFY2(rentTerm.exec(), qPrintable(rentTerm.lastError().text()));

    QCOMPARE(countRows("SELECT count(*) FROM vwcurrentrentals WHERE terminalid = " + QString::number(termId)), 1);

    QString payNum = generateNumber("payment");
    QVERIFY2(payNum.startsWith("ОП-"), qPrintable(payNum));
    QSqlQuery pay(m_testDb);
    pay.prepare("INSERT INTO tblpayments (clientid, periodmonth, periodyear, amount) "
                "VALUES (:c, 7, 2026, 100.00) RETURNING paymentid");
    pay.bindValue(":c", clientId);
    QVERIFY2(pay.exec(), qPrintable(pay.lastError().text()));
    QVERIFY(pay.next());
    int payId = pay.value(0).toInt();

    QSqlQuery link(m_testDb);
    link.prepare("INSERT INTO tblpayment_rental_links (paymentid, rentaldocid) VALUES (:p, :r)");
    link.bindValue(":p", payId);
    link.bindValue(":r", rentalId);
    QVERIFY2(link.exec(), qPrintable(link.lastError().text()));

    QSqlQuery dupPay(m_testDb);
    dupPay.prepare("INSERT INTO tblpayments (clientid, periodmonth, periodyear) VALUES (:c, 7, 2026)");
    dupPay.bindValue(":c", clientId);
    QVERIFY(!dupPay.exec());

    QString retNum = generateNumber("return");
    QVERIFY2(retNum.startsWith("ВР-"), qPrintable(retNum));
    QSqlQuery retDoc(m_testDb);
    retDoc.prepare("INSERT INTO tblreturndocs (docnumber, clientid) VALUES (:n, :c) RETURNING returndocid");
    retDoc.bindValue(":n", retNum);
    retDoc.bindValue(":c", clientId);
    QVERIFY2(retDoc.exec(), qPrintable(retDoc.lastError().text()));
    QVERIFY(retDoc.next());
    int retId = retDoc.value(0).toInt();

    QSqlQuery retDetail(m_testDb);
    retDetail.prepare("INSERT INTO tblreturndetails (returndocid, terminalid) VALUES (:d, :t)");
    retDetail.bindValue(":d", retId);
    retDetail.bindValue(":t", termId);
    QVERIFY2(retDetail.exec(), qPrintable(retDetail.lastError().text()));

    QSqlQuery retTerm(m_testDb);
    retTerm.prepare("UPDATE tblterminals SET status = 0, currentsimcardid = NULL WHERE terminalid = :t");
    retTerm.bindValue(":t", termId);
    QVERIFY2(retTerm.exec(), qPrintable(retTerm.lastError().text()));

    QCOMPARE(countRows("SELECT count(*) FROM vwcurrentrentals WHERE terminalid = " + QString::number(termId)), 0);

    QSqlQuery full(m_testDb);
    full.prepare("SELECT terminalstatusname FROM vwterminalsfull WHERE terminalid = :t");
    full.bindValue(":t", termId);
    QVERIFY2(full.exec(), qPrintable(full.lastError().text()));
    QVERIFY(full.next());
    QCOMPARE(full.value(0).toString(), QString("Свободен"));

    int auditCount = countRows("SELECT count(*) FROM tbl_audit_log "
                               "WHERE table_name = 'tblterminals' AND record_id = " +
                               QString::number(termId));
    QVERIFY2(auditCount >= 3,
             qPrintable("Аудит терминала: ожидалось >= 3 записей, получено " + QString::number(auditCount)));
}