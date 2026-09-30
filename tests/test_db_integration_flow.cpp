#include "test_db_integration.h"

#include <QDate>
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

    // Клиент может платить дважды в месяце: вторая оплата за тот же период
    // добавляется, а не заменяет первую (UNIQUE снят миграцией 016).
    QSqlQuery secondPay(m_testDb);
    secondPay.prepare("INSERT INTO tblpayments (clientid, periodmonth, periodyear, amount) "
                      "VALUES (:c, 7, 2026, 250.50) RETURNING paymentid");
    secondPay.bindValue(":c", clientId);
    QVERIFY2(secondPay.exec(), qPrintable(secondPay.lastError().text()));
    QVERIFY(secondPay.next());
    int secondPayId = secondPay.value(0).toInt();
    QVERIFY(secondPayId != payId);

    // Первая оплата сохранилась — обе лежат в одном периоде
    QCOMPARE(countRows("SELECT count(*) FROM tblpayments WHERE clientid = " + QString::number(clientId) +
                       " AND periodmonth = 7 AND periodyear = 2026"),
             2);

    // Обе оплаты привязаны к тому же документу аренды — их суммы складываются
    // (такой запрос делает PaymentRepository::paidByRentalDocs для колонки
    // «Оплачено» в форме отметки оплаты).
    QSqlQuery secondLink(m_testDb);
    secondLink.prepare("INSERT INTO tblpayment_rental_links (paymentid, rentaldocid) VALUES (:p, :r)");
    secondLink.bindValue(":p", secondPayId);
    secondLink.bindValue(":r", rentalId);
    QVERIFY2(secondLink.exec(), qPrintable(secondLink.lastError().text()));

    QSqlQuery paidSum(m_testDb);
    paidSum.prepare("SELECT COALESCE(SUM(p.amount), 0) FROM tblpayment_rental_links pl "
                    "JOIN tblpayments p ON p.paymentid = pl.paymentid "
                    "WHERE p.clientid = :c AND pl.rentaldocid = :r");
    paidSum.bindValue(":c", clientId);
    paidSum.bindValue(":r", rentalId);
    QVERIFY2(paidSum.exec(), qPrintable(paidSum.lastError().text()));
    QVERIFY(paidSum.next());
    QCOMPARE(paidSum.value(0).toDouble(), 350.50);

    // Агрегация отчёта «Выручка по клиентам»: у клиента две строки аренды и две
    // оплаты — сумма оплат не должна умножаться на число строк аренды
    // (запрос из ReportsForm::generateRevenueByClient, reportsform.cpp).
    QSqlQuery term2(m_testDb);
    term2.prepare("INSERT INTO tblterminals (serialnumber, modelid, status) "
                  "VALUES (:s, :m, 1) RETURNING terminalid");
    term2.bindValue(":s", "ТЕРМ-ТЕСТ-002");
    term2.bindValue(":m", modelId);
    QVERIFY2(term2.exec(), qPrintable(term2.lastError().text()));
    QVERIFY(term2.next());

    QSqlQuery rentDetail2(m_testDb);
    rentDetail2.prepare("INSERT INTO tblrentaldetails (rentaldocid, terminalid) VALUES (:d, :t)");
    rentDetail2.bindValue(":d", rentalId);
    rentDetail2.bindValue(":t", term2.value(0).toInt());
    QVERIFY2(rentDetail2.exec(), qPrintable(rentDetail2.lastError().text()));

    QSqlQuery revenue(m_testDb);
    revenue.prepare("SELECT COALESCE(p.payment_cnt, 0), COALESCE(p.total, 0), COALESCE(r.terminal_cnt, 0) "
                    "FROM tblclients c "
                    "LEFT JOIN (SELECT clientid, COUNT(*) AS payment_cnt, SUM(amount) AS total "
                    "           FROM tblpayments "
                    "           WHERE paymentdate >= :dateFrom::date "
                    "             AND paymentdate < :dateTo::date + interval '1 day' "
                    "           GROUP BY clientid) p ON p.clientid = c.clientid "
                    "LEFT JOIN (SELECT r.clientid, COUNT(DISTINCT rd.terminalid) AS terminal_cnt "
                    "           FROM tblrentaldetails rd "
                    "           JOIN tblrentaldocs r ON r.rentaldocid = rd.rentaldocid "
                    "           GROUP BY r.clientid) r ON r.clientid = c.clientid "
                    "WHERE c.clientid = :c");
    revenue.bindValue(":dateFrom", "2000-01-01");
    revenue.bindValue(":dateTo", QDate::currentDate().toString("yyyy-MM-dd"));
    revenue.bindValue(":c", clientId);
    QVERIFY2(revenue.exec(), qPrintable(revenue.lastError().text()));
    QVERIFY(revenue.next());
    QCOMPARE(revenue.value(0).toInt(), 2);         // платежей
    QCOMPARE(revenue.value(1).toDouble(), 350.50); // 100.00 + 250.50, не ×2 строки аренды
    QCOMPARE(revenue.value(2).toInt(), 2);         // терминалов в аренде

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