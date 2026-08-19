#include "test_db_integration.h"

#include <QSqlError>
#include <QStringList>
#include <QtTest>

void TestDbIntegration::test_schema_objects()
{
    const QStringList expectedTables = {"tblclients",
                                        "tblmanufacturers",
                                        "tblmodels",
                                        "tblsimcards",
                                        "tblterminals",
                                        "tblreceiptdocs",
                                        "tblreceiptdetails",
                                        "tblrentaldocs",
                                        "tblrentaldetails",
                                        "tblreturndocs",
                                        "tblreturndetails",
                                        "tblpayments",
                                        "tblpayment_rental_links",
                                        "tblsimassignments",
                                        "tblstatuschangedocs",
                                        "tblstatuschangedetails",
                                        "tbl_users",
                                        "tbl_audit_log",
                                        "schema_migrations"};
    QString err;
    QSqlQuery q = querySql("SELECT tablename FROM pg_tables WHERE schemaname = 'public'", &err);
    QVERIFY2(q.lastError().driverText().isEmpty(), qPrintable("pg_tables: " + err));
    QStringList found;
    while (q.next())
        found << q.value(0).toString();
    for (const QString& t : expectedTables)
        QVERIFY2(found.contains(t), qPrintable("Нет таблицы: " + t));

    QSqlQuery v = querySql("SELECT viewname FROM pg_views WHERE schemaname = 'public'", &err);
    QVERIFY2(v.lastError().driverText().isEmpty(), qPrintable("pg_views: " + err));
    found.clear();
    while (v.next())
        found << v.value(0).toString();
    for (const QString& name : {QString("vwterminalsfull"), QString("vwcurrentrentals"), QString("vsimcards")})
        QVERIFY2(found.contains(name), qPrintable("Нет представления: " + name));

    QSqlQuery c = querySql("SELECT column_name FROM information_schema.columns WHERE table_name = 'tbl_users'", &err);
    QVERIFY2(c.lastError().driverText().isEmpty(), qPrintable("columns: " + err));
    found.clear();
    while (c.next())
        found << c.value(0).toString();
    for (const QString& col : {QString("failed_login_attempts"), QString("locked_until")})
        QVERIFY2(found.contains(col), qPrintable("Нет колонки: " + col));

    QSqlQuery s =
        querySql("SELECT sequence_name FROM information_schema.sequences WHERE sequence_schema = 'public'", &err);
    QVERIFY2(s.lastError().driverText().isEmpty(), qPrintable("sequences: " + err));
    found.clear();
    while (s.next())
        found << s.value(0).toString();
    for (const QString& seq :
         {QString("seq_receipt_doc_number"), QString("seq_rental_doc_number"), QString("seq_return_doc_number"),
          QString("seq_payment_doc_number"), QString("seq_statuschange_doc_number")})
        QVERIFY2(found.contains(seq), qPrintable("Нет последовательности: " + seq));

    QCOMPARE(countRows("SELECT count(*) FROM schema_migrations"), 14);
}

void TestDbIntegration::test_number_generation()
{
    struct {
        QString type;
        QString prefix;
    } cases[] = {{"receipt", "ПП-"}, {"rental", "АР-"}, {"return", "ВР-"}, {"payment", "ОП-"}, {"statuschange", "ИС-"}};
    for (const auto& c : cases) {
        bool ok = false;
        QString num = generateNumber(c.type, &ok);
        QVERIFY2(ok, qPrintable("Ошибка генерации номера: " + c.type));
        QVERIFY2(num.startsWith(c.prefix), qPrintable(num));
    }

    bool ok1 = false, ok2 = false;
    QString n1 = generateNumber("receipt", &ok1);
    QString n2 = generateNumber("receipt", &ok2);
    QVERIFY(ok1 && ok2);
    QVERIFY(n1 != n2);

    QSqlQuery bad(m_testDb);
    bad.prepare("SELECT generate_doc_number(:t)");
    bad.bindValue(":t", "unknown_type");
    QVERIFY(!bad.exec());
    QVERIFY(bad.lastError().text().contains("Неизвестный тип документа"));
}

void TestDbIntegration::test_unique_docnumber()
{
    QSqlQuery ins(m_testDb);
    ins.prepare("INSERT INTO tblreceiptdocs (docnumber) VALUES (:n)");
    ins.bindValue(":n", "ПП-UNIQ-1");
    QVERIFY2(ins.exec(), qPrintable(ins.lastError().text()));

    QSqlQuery dup(m_testDb);
    dup.prepare("INSERT INTO tblreceiptdocs (docnumber) VALUES (:n)");
    dup.bindValue(":n", "ПП-UNIQ-1");
    QVERIFY(!dup.exec());
}

void TestDbIntegration::test_audit_triggers()
{
    setAppValue("app.username", "tester");

    QSqlQuery insClient(m_testDb);
    insClient.prepare("INSERT INTO tblclients (clientname) VALUES (:n) RETURNING clientid");
    insClient.bindValue(":n", "Аудит-Клиент");
    QVERIFY2(insClient.exec(), qPrintable(insClient.lastError().text()));
    QVERIFY(insClient.next());
    int clientId = insClient.value(0).toInt();

    QCOMPARE(countRows("SELECT count(*) FROM tbl_audit_log "
                       "WHERE table_name = 'tblclients' AND record_id = " +
                       QString::number(clientId) + " AND action = 'CREATE' AND username = 'tester'"),
             1);

    QSqlQuery updClient(m_testDb);
    updClient.prepare("UPDATE tblclients SET clientname = 'Аудит-Клиент-2' WHERE clientid = :id");
    updClient.bindValue(":id", clientId);
    QVERIFY2(updClient.exec(), qPrintable(updClient.lastError().text()));
    QCOMPARE(countRows("SELECT count(*) FROM tbl_audit_log "
                       "WHERE table_name = 'tblclients' AND record_id = " +
                       QString::number(clientId) + " AND action = 'UPDATE'"),
             1);

    QSqlQuery delClient(m_testDb);
    delClient.prepare("DELETE FROM tblclients WHERE clientid = :id");
    delClient.bindValue(":id", clientId);
    QVERIFY2(delClient.exec(), qPrintable(delClient.lastError().text()));
    QCOMPARE(countRows("SELECT count(*) FROM tbl_audit_log "
                       "WHERE table_name = 'tblclients' AND record_id = " +
                       QString::number(clientId) + " AND action = 'DELETE'"),
             1);

    QSqlQuery man(m_testDb);
    man.prepare("INSERT INTO tblmanufacturers (manufacturername) VALUES (:n) RETURNING manufacturerid");
    man.bindValue(":n", "Аудит-Производитель");
    QVERIFY2(man.exec(), qPrintable(man.lastError().text()));
    QVERIFY(man.next());
    int manId = man.value(0).toInt();

    QSqlQuery model(m_testDb);
    model.prepare("INSERT INTO tblmodels (manufacturerid, modelname) VALUES (:m, :n) RETURNING modelid");
    model.bindValue(":m", manId);
    model.bindValue(":n", "Аудит-Модель");
    QVERIFY2(model.exec(), qPrintable(model.lastError().text()));
    QVERIFY(model.next());
    int modelId = model.value(0).toInt();

    QSqlQuery term(m_testDb);
    term.prepare("INSERT INTO tblterminals (serialnumber, modelid) VALUES (:s, :m) RETURNING terminalid");
    term.bindValue(":s", "АУДИТ-ТЕРМ-001");
    term.bindValue(":m", modelId);
    QVERIFY2(term.exec(), qPrintable(term.lastError().text()));
    QVERIFY(term.next());
    int termId = term.value(0).toInt();

    QCOMPARE(countRows("SELECT count(*) FROM tbl_audit_log "
                       "WHERE table_name = 'tblterminals' AND record_id = " +
                       QString::number(termId) + " AND action = 'CREATE' AND username = 'tester'"),
             1);
}

void TestDbIntegration::test_role_enforcement()
{
    QVERIFY(execSql("DELETE FROM tbl_users WHERE username = 'manager'"));
    QSqlQuery ins(m_testDb);
    ins.prepare("INSERT INTO tbl_users (username, role, is_active, password_hash) "
                "VALUES ('manager', 'user', TRUE, 'hash') RETURNING user_id");
    QVERIFY2(ins.exec(), qPrintable(ins.lastError().text()));
    QVERIFY(ins.next());

    setAppValue("app.role", "user");
    setAppValue("app.username", "manager");

    QSqlQuery adminUpd(m_testDb);
    adminUpd.prepare("UPDATE tbl_users SET role = 'user' WHERE username = 'admin'");
    QVERIFY(!adminUpd.exec());
    QVERIFY(adminUpd.lastError().text().contains("Доступ запрещён"));

    QSqlQuery ownPass(m_testDb);
    ownPass.prepare("UPDATE tbl_users SET password_hash = 'newhash' WHERE username = 'manager'");
    QVERIFY2(ownPass.exec(), qPrintable(ownPass.lastError().text()));

    QSqlQuery ownRole(m_testDb);
    ownRole.prepare("UPDATE tbl_users SET role = 'admin' WHERE username = 'manager'");
    QVERIFY(!ownRole.exec());

    QSqlQuery insUser(m_testDb);
    insUser.prepare("INSERT INTO tbl_users (username, role) VALUES ('newbie', 'user')");
    QVERIFY(!insUser.exec());

    QSqlQuery delUser(m_testDb);
    delUser.prepare("DELETE FROM tbl_users WHERE username = 'manager'");
    QVERIFY(!delUser.exec());

    QSqlQuery insAudit(m_testDb);
    insAudit.prepare("INSERT INTO tbl_audit_log (username, action) VALUES ('x', 'TEST')");
    QVERIFY2(insAudit.exec(), qPrintable(insAudit.lastError().text()));

    QSqlQuery updAudit(m_testDb);
    updAudit.prepare("UPDATE tbl_audit_log SET action = 'Y'");
    QVERIFY(!updAudit.exec());
    QVERIFY(updAudit.lastError().text().contains("Доступ запрещён"));

    QSqlQuery delAudit(m_testDb);
    delAudit.prepare("DELETE FROM tbl_audit_log");
    QVERIFY(!delAudit.exec());

    setAppValue("app.role", "admin");

    QSqlQuery adminIns(m_testDb);
    adminIns.prepare("INSERT INTO tbl_users (username, role) VALUES ('cleanup_user', 'user')");
    QVERIFY2(adminIns.exec(), qPrintable(adminIns.lastError().text()));

    QSqlQuery adminDel(m_testDb);
    adminDel.prepare("DELETE FROM tbl_users WHERE username = 'cleanup_user'");
    QVERIFY2(adminDel.exec(), qPrintable(adminDel.lastError().text()));
}

void TestDbIntegration::test_rate_limiting()
{
    QVERIFY(execSql("DELETE FROM tbl_users WHERE username = 'lockuser'"));
    QVERIFY(execSql("INSERT INTO tbl_users (username, role, is_active) VALUES ('lockuser', 'user', TRUE)"));

    QSqlQuery sel(m_testDb);
    sel.prepare("SELECT failed_login_attempts, locked_until FROM tbl_users WHERE username = 'lockuser'");
    QVERIFY2(sel.exec(), qPrintable(sel.lastError().text()));
    QVERIFY(sel.next());
    QCOMPARE(sel.value(0).toInt(), 0);
    QVERIFY(sel.value(1).isNull());

    QVERIFY(execSql("UPDATE tbl_users SET failed_login_attempts = 4, locked_until = NULL "
                    "WHERE username = 'lockuser'"));

    QSqlQuery up(m_testDb);
    up.prepare("UPDATE tbl_users "
               "SET failed_login_attempts = failed_login_attempts + 1, "
               "locked_until = CASE WHEN failed_login_attempts + 1 >= 5 "
               "THEN NOW() + INTERVAL '30 seconds' ELSE locked_until END "
               "WHERE username = 'lockuser' RETURNING failed_login_attempts, locked_until");
    QVERIFY2(up.exec(), qPrintable(up.lastError().text()));
    QVERIFY(up.next());
    QCOMPARE(up.value(0).toInt(), 5);
    QVERIFY(!up.value(1).isNull());

    QVERIFY(execSql("UPDATE tbl_users SET failed_login_attempts = 0, locked_until = NULL "
                    "WHERE username = 'lockuser'"));

    QSqlQuery sel2(m_testDb);
    sel2.prepare("SELECT failed_login_attempts, locked_until FROM tbl_users WHERE username = 'lockuser'");
    QVERIFY2(sel2.exec(), qPrintable(sel2.lastError().text()));
    QVERIFY(sel2.next());
    QCOMPARE(sel2.value(0).toInt(), 0);
    QVERIFY(sel2.value(1).isNull());
}