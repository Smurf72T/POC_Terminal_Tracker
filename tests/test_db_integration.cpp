#include <QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QDateTime>
#include <QCoreApplication>
#include "ops/backupmanager.h"
#include "ops/opslog.h"

class TestDbIntegration : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void test_schema_objects();
    void test_number_generation();
    void test_unique_docnumber();
    void test_audit_triggers();
    void test_role_enforcement();
    void test_rate_limiting();
    void test_business_flow();
    void test_backup_and_opslog();

private:
    QSqlDatabase m_testDb;
    QSqlDatabase m_adminDb;
    QString m_testDbName = "pocbase_test";

    QMap<QString, QString> m_env;
    QJsonObject m_dbConfig;

    bool applyMigrations();
    bool execSql(const QString& sql, QString* err = nullptr);
    QSqlQuery querySql(const QString& sql, QString* err = nullptr);
    int countRows(const QString& sql, QString* err = nullptr);
    QString generateNumber(const QString& docType, bool* ok = nullptr);
    void setAppValue(const QString& key, const QString& value);
};

static QMap<QString, QString> loadEnvFile(const QString& filePath)
{
    QMap<QString, QString> env;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return env;

    while (!file.atEnd()) {
        QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;
        int eq = line.indexOf('=');
        if (eq < 0)
            continue;
        env.insert(line.left(eq).trimmed(), line.mid(eq + 1).trimmed());
    }
    return env;
}

static QString envValue(const QMap<QString, QString>& env, const QJsonObject& cfg, const QString& envKey,
                        const QString& cfgKey, const QString& def)
{
    if (qEnvironmentVariableIsSet(envKey.toUtf8().constData()))
        return qEnvironmentVariable(envKey.toUtf8().constData());
    if (env.contains(envKey))
        return env[envKey];
    if (cfg.contains(cfgKey))
        return cfg[cfgKey].toString();
    return def;
}

static bool openConnection(QSqlDatabase& db, const QString& dbName, const QMap<QString, QString>& env,
                           const QJsonObject& cfg, QString* err)
{
    db.setHostName(envValue(env, cfg, "POC_DB_HOST", "host", "localhost"));
    db.setPort(envValue(env, cfg, "POC_DB_PORT", "port", "5432").toInt());
    db.setDatabaseName(dbName);
    db.setUserName(envValue(env, cfg, "POC_DB_USER", "username", "postgres"));
    db.setPassword(envValue(env, cfg, "POC_DB_PASSWORD", "password", ""));
    db.setConnectOptions("sslmode=disable");
    if (!db.open()) {
        if (err)
            *err = db.lastError().text();
        return false;
    }
    return true;
}

void TestDbIntegration::initTestCase()
{
    QString appDir = QCoreApplication::applicationDirPath();

    QStringList envCandidates = {appDir + "/.env", appDir + "/../.env", appDir + "/config/.env",
                                 appDir + "/../../.env"};
    for (const QString& candidate : envCandidates) {
        m_env = loadEnvFile(candidate);
        if (!m_env.isEmpty())
            break;
    }

    QString configPath = appDir + "/config/config.json";
    QFile file(configPath);
    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (!doc.isNull())
            m_dbConfig = doc.object()["database"].toObject();
    }

    if (qEnvironmentVariableIsSet("POC_TEST_DB_NAME"))
        m_testDbName = qEnvironmentVariable("POC_TEST_DB_NAME");

    m_adminDb = QSqlDatabase::addDatabase("QPSQL", "adminConnection");
    QString adminErr;
    if (!openConnection(m_adminDb, "postgres", m_env, m_dbConfig, &adminErr)) {
        QSKIP(QString("PostgreSQL недоступен (не могу подключиться к базе 'postgres'): %1").arg(adminErr).toUtf8());
    }

    QSqlQuery term(m_adminDb);
    term.exec(QString("SELECT pg_terminate_backend(pid) FROM pg_stat_activity "
                      "WHERE datname = '%1' AND pid <> pg_backend_pid()")
                  .arg(m_testDbName));

    QSqlQuery drop(m_adminDb);
    if (!drop.exec(QString("DROP DATABASE IF EXISTS %1").arg(m_testDbName))) {
        QSKIP(QString("Не удалось сбросить тестовую БД %1: %2").arg(m_testDbName, drop.lastError().text()).toUtf8());
    }

    QSqlQuery create(m_adminDb);
    if (!create.exec(QString("CREATE DATABASE %1 ENCODING 'UTF8'").arg(m_testDbName))) {
        QSKIP(QString("Нет прав на создание тестовой БД %1: %2").arg(m_testDbName, create.lastError().text()).toUtf8());
    }

    m_testDb = QSqlDatabase::addDatabase("QPSQL", "testConnection");
    QString testErr;
    if (!openConnection(m_testDb, m_testDbName, m_env, m_dbConfig, &testErr)) {
        QSKIP(QString("Не удалось подключиться к тестовой БД %1: %2").arg(m_testDbName, testErr).toUtf8());
    }

    if (!applyMigrations()) {
        QFAIL("Не удалось применить миграции к тестовой БД");
    }
}

void TestDbIntegration::cleanupTestCase()
{
    m_testDb.close();
    QSqlDatabase::removeDatabase("testConnection");

    if (m_adminDb.isOpen()) {
        QSqlQuery term(m_adminDb);
        term.exec(QString("SELECT pg_terminate_backend(pid) FROM pg_stat_activity "
                          "WHERE datname = '%1' AND pid <> pg_backend_pid()")
                      .arg(m_testDbName));
        QSqlQuery drop(m_adminDb);
        if (!drop.exec(QString("DROP DATABASE IF EXISTS %1").arg(m_testDbName)))
            qWarning() << "Не удалось удалить тестовую БД" << m_testDbName << ":" << drop.lastError().text();
        m_adminDb.close();
    }
    QSqlDatabase::removeDatabase("adminConnection");
}

bool TestDbIntegration::applyMigrations()
{
    QString migrationsDir;
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidates = {appDir + "/sql/migrations/", appDir + "/../sql/migrations/",
                              appDir + "/../../sql/migrations/"};
    for (const QString& c : candidates) {
        QDir d(c);
        if (d.exists()) {
            migrationsDir = d.absolutePath();
            break;
        }
    }
    if (migrationsDir.isEmpty())
        return false;

    QSqlQuery schemaQ(m_testDb);
    if (!schemaQ.exec("CREATE TABLE IF NOT EXISTS schema_migrations ("
                      "  version VARCHAR(255) PRIMARY KEY,"
                      "  applied_at TIMESTAMP DEFAULT NOW()"
                      ")")) {
        return false;
    }

    QStringList pending;
    QDirIterator it(migrationsDir, QStringList() << "*.sql", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        pending.append(it.filePath());
    }
    pending.sort();

    for (const QString& filePath : pending) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;
        QString sql = QString::fromUtf8(file.readAll());
        file.close();

        if (!m_testDb.transaction())
            return false;

        QSqlQuery q(m_testDb);
        if (!q.exec(sql)) {
            qWarning() << "Ошибка в миграции" << QFileInfo(filePath).fileName() << ":" << q.lastError().text();
            m_testDb.rollback();
            return false;
        }

        QSqlQuery rec(m_testDb);
        rec.prepare("INSERT INTO schema_migrations (version) VALUES (:v)");
        rec.bindValue(":v", QFileInfo(filePath).fileName());
        if (!rec.exec()) {
            qWarning() << "Не удалось записать версию миграции" << QFileInfo(filePath).fileName() << ":"
                       << rec.lastError().text();
            m_testDb.rollback();
            return false;
        }

        if (!m_testDb.commit()) {
            m_testDb.rollback();
            return false;
        }
    }

    // Миграции больше не создают дефолтного admin (пароль не хранится в репозитории,
    // P0-3). Для тестов ролевой политики seed'им admin напрямую.
    QSqlQuery seedAdmin(m_testDb);
    seedAdmin.prepare("INSERT INTO tbl_users (username, display_name, password_hash, role, is_active) "
                      "VALUES ('admin', 'Администратор', 'test', 'admin', TRUE) "
                      "ON CONFLICT (username) DO NOTHING");
    if (!seedAdmin.exec()) {
        qWarning() << "Не удалось создать admin для тестов:" << seedAdmin.lastError().text();
        return false;
    }

    return true;
}

bool TestDbIntegration::execSql(const QString& sql, QString* err)
{
    QSqlQuery q(m_testDb);
    bool ok = q.exec(sql);
    if (!ok && err)
        *err = q.lastError().text();
    return ok;
}

QSqlQuery TestDbIntegration::querySql(const QString& sql, QString* err)
{
    QSqlQuery q(m_testDb);
    q.exec(sql);
    if (err && q.lastError().isValid())
        *err = q.lastError().text();
    return q;
}

int TestDbIntegration::countRows(const QString& sql, QString* err)
{
    QSqlQuery q = querySql(sql, err);
    if (!q.next())
        return -1;
    return q.value(0).toInt();
}

QString TestDbIntegration::generateNumber(const QString& docType, bool* ok)
{
    QSqlQuery q(m_testDb);
    q.prepare("SELECT generate_doc_number(:t)");
    q.bindValue(":t", docType);
    if (!q.exec() || !q.next()) {
        if (ok)
            *ok = false;
        return QString();
    }
    if (ok)
        *ok = true;
    return q.value(0).toString();
}

void TestDbIntegration::setAppValue(const QString& key, const QString& value)
{
    QSqlQuery q(m_testDb);
    q.prepare("SELECT set_config(:key, :value, false)");
    q.bindValue(":key", key);
    q.bindValue(":value", value);
    q.exec();
}

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

    QCOMPARE(countRows("SELECT count(*) FROM schema_migrations"), 12);
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

void TestDbIntegration::test_backup_and_opslog()
{
    QString tempDir = QDir::temp().filePath("poc_ops_test");
    QDir(tempDir).mkpath(".");
    QString dbname = m_testDb.databaseName();

    // Fallback-дамп не зависит от внешних утилит и должен содержать структуру и данные
    QString err;
    QString backupFile = tempDir + "/backup_test.sql";
    QVERIFY2(BackupManager::createFallbackBackup(m_testDb, backupFile, dbname, &err), qPrintable(err));
    QVERIFY2(QFileInfo(backupFile).size() > 0, qPrintable("fallback-дамп пустой"));
    QFile f(backupFile);
    QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
    QString content = QString::fromUtf8(f.readAll());
    f.close();
    QVERIFY2(content.contains("CREATE TABLE"), qPrintable("дамп не содержит CREATE TABLE"));
    QVERIFY2(content.contains("INSERT INTO"), qPrintable("дамп не содержит данных"));

    // createBackup: pg_dump (если доступен в PATH) либо fallback — в любом случае файл создаётся
    QString backupFull = tempDir + "/backup_full.sql";
    BackupManager::BackupResult result =
        BackupManager::createBackup(m_testDb, backupFull, m_testDb.password(), m_testDb.password());
    QVERIFY2(result.ok, qPrintable(result.error));
    QVERIFY2(result.size > 0, qPrintable("бэкап пустой"));
    QVERIFY2(result.method == "pg_dump" || result.method == "fallback",
             qPrintable("неизвестный метод бэкапа: " + result.method));

    // Шифрование: бэкап, созданный с паролем, начинается с маркера POCENC1
    QVERIFY2(result.encrypted, qPrintable("бэкап с паролем должен быть зашифрован"));
    QFile enc(backupFull);
    QVERIFY2(enc.open(QIODevice::ReadOnly), qPrintable("не удалось открыть зашифрованный бэкап"));
    QCOMPARE(QString::fromUtf8(enc.read(8)), QString("POCENC1\n"));
    enc.close();

    // Roundtrip: восстановление зашифрованного бэкапа в СВЕЖУЮ пустую БД.
    // (Восстановление поверх рабочей БД не поддерживается: pg_dump --clean
    //  не удаляет таблицы с внешнеключевыми зависимостями.)
    const QString restoreDbName = "pocbase_test_restore";
    QSqlQuery termR(m_adminDb);
    termR.exec(QString("SELECT pg_terminate_backend(pid) FROM pg_stat_activity "
                       "WHERE datname = '%1' AND pid <> pg_backend_pid()")
                   .arg(restoreDbName));
    QSqlQuery dropR(m_adminDb);
    QVERIFY2(dropR.exec(QString("DROP DATABASE IF EXISTS %1").arg(restoreDbName)),
             qPrintable(dropR.lastError().text()));
    QSqlQuery createR(m_adminDb);
    QVERIFY2(createR.exec(QString("CREATE DATABASE %1 ENCODING 'UTF8'").arg(restoreDbName)),
             qPrintable(createR.lastError().text()));

    {
        QSqlDatabase restoreDb = QSqlDatabase::addDatabase("QPSQL", "restoreConnection");
        QString rErr;
        QVERIFY2(openConnection(restoreDb, restoreDbName, m_env, m_dbConfig, &rErr), qPrintable(rErr));
        QString restoreErr;
        QVERIFY2(BackupManager::restoreDatabase(restoreDb, backupFull, m_testDb.password(), m_testDb.password(),
                                                &restoreErr),
                 qPrintable(restoreErr));
        restoreDb.close();
        QSqlDatabase::removeDatabase("restoreConnection");
    }

    // В восстановленной БД должны быть схема (миграции) и данные
    {
        QSqlDatabase check = QSqlDatabase::addDatabase("QPSQL", "restoreCheckConnection");
        QString cErr;
        QVERIFY2(openConnection(check, restoreDbName, m_env, m_dbConfig, &cErr), qPrintable(cErr));
        QSqlQuery q(check);
        QVERIFY2(q.exec("SELECT count(*) FROM tbl_users"), qPrintable(q.lastError().text()));
        QVERIFY(q.next());
        QVERIFY2(q.value(0).toInt() > 0, qPrintable("в восстановленной БД нет пользователей"));
        QSqlQuery mq(check);
        QVERIFY(mq.exec("SELECT count(*) FROM schema_migrations"));
        QVERIFY(mq.next());
        QCOMPARE(mq.value(0).toInt(), 12);
        check.close();
        QSqlDatabase::removeDatabase("restoreCheckConnection");
    }

    QSqlQuery termR2(m_adminDb);
    termR2.exec(QString("SELECT pg_terminate_backend(pid) FROM pg_stat_activity "
                        "WHERE datname = '%1' AND pid <> pg_backend_pid()")
                    .arg(restoreDbName));
    QSqlQuery dropR2(m_adminDb);
    dropR2.exec(QString("DROP DATABASE IF EXISTS %1").arg(restoreDbName));

    // Бэкап без пароля — plaintext SQL (обратная совместимость)
    QString backupPlain = tempDir + "/backup_plain.sql";
    BackupManager::BackupResult plainResult =
        BackupManager::createBackup(m_testDb, backupPlain, m_testDb.password(), QString());
    QVERIFY2(plainResult.ok, qPrintable(plainResult.error));
    QVERIFY2(!plainResult.encrypted, qPrintable("бэкап без пароля не должен быть зашифрован"));
    QFile pf(backupPlain);
    QVERIFY(pf.open(QIODevice::ReadOnly | QIODevice::Text));
    QString plainContent = QString::fromUtf8(pf.readAll());
    pf.close();
    QVERIFY2(plainContent.contains("CREATE TABLE"), qPrintable("plain-бэкап не содержит CREATE TABLE"));

    // Журнал операций: запись должна появиться в ops.log
    OpsLog::instance().setLogDirectory(tempDir);
    QString msg =
        "Тестовая запись журнала операций " + QString::number(QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
    OpsLog::instance().info(msg);
    QString logPath = OpsLog::instance().logFilePath();
    QVERIFY2(QFile::exists(logPath), qPrintable("ops.log не создан: " + logPath));
    QFile lf(logPath);
    QVERIFY(lf.open(QIODevice::ReadOnly | QIODevice::Text));
    QString logContent = QString::fromUtf8(lf.readAll());
    lf.close();
    QVERIFY2(logContent.contains(msg), qPrintable("ops.log не содержит запись"));

    QDir(tempDir).removeRecursively();
}

QTEST_GUILESS_MAIN(TestDbIntegration)
#include "test_db_integration.moc"
