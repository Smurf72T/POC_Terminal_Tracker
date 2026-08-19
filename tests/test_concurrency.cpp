#include "test_concurrency.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSqlError>
#include <QtTest>

// ==================== TestCase: init / cleanup / вспомогательные ====================

void TestConcurrency::initTestCase()
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

    m_adminDb = QSqlDatabase::addDatabase("QPSQL", "concurrencyAdminConnection");
    QString adminErr;
    if (!openConnection(m_adminDb, "postgres", m_env, m_dbConfig, &adminErr)) {
        QSKIP(QString("PostgreSQL недоступен: %1").arg(adminErr).toUtf8());
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

    m_testDb = QSqlDatabase::addDatabase("QPSQL", "concurrencyMainConnection");
    QString testErr;
    if (!openConnection(m_testDb, m_testDbName, m_env, m_dbConfig, &testErr)) {
        QSKIP(QString("Не удалось подключиться к тестовой БД %1: %2").arg(m_testDbName, testErr).toUtf8());
    }

    if (!applyMigrations())
        QFAIL("Не удалось применить миграции к тестовой БД");

    // Общая модель для всех тестовых терминалов
    QSqlQuery man(m_testDb);
    QVERIFY2(man.exec("INSERT INTO tblmanufacturers (manufacturername) "
                      "VALUES ('Конкурентный-Производитель') RETURNING manufacturerid"),
             qPrintable(man.lastError().text()));
    QVERIFY(man.next());
    int manId = man.value(0).toInt();

    QSqlQuery model(m_testDb);
    model.prepare("INSERT INTO tblmodels (manufacturerid, modelname) VALUES (:m, :n) RETURNING modelid");
    model.bindValue(":m", manId);
    model.bindValue(":n", "Конкурентная-Модель");
    QVERIFY2(model.exec(), qPrintable(model.lastError().text()));
    QVERIFY(model.next());
    m_modelId = model.value(0).toInt();
}

void TestConcurrency::cleanupTestCase()
{
    m_testDb.close();
    QSqlDatabase::removeDatabase("concurrencyMainConnection");

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
    QSqlDatabase::removeDatabase("concurrencyAdminConnection");
}

bool TestConcurrency::applyMigrations()
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
        if (!rec.exec() || !m_testDb.commit()) {
            m_testDb.rollback();
            return false;
        }
    }
    return true;
}

bool TestConcurrency::execOnTest(const QString& sql, QString* err)
{
    QSqlQuery q(m_testDb);
    bool ok = q.exec(sql);
    if (!ok && err)
        *err = q.lastError().text();
    return ok;
}

QSqlQuery TestConcurrency::queryOnTest(const QString& sql, QString* err)
{
    QSqlQuery q(m_testDb);
    q.exec(sql);
    if (err && q.lastError().isValid())
        *err = q.lastError().text();
    return q;
}

int TestConcurrency::countOnTest(const QString& sql)
{
    QSqlQuery q = queryOnTest(sql);
    if (!q.next())
        return -1;
    return q.value(0).toInt();
}

int TestConcurrency::seedTerminal(const QString& serial, int status)
{
    QSqlQuery q(m_testDb);
    q.prepare("INSERT INTO tblterminals (serialnumber, modelid, status) "
              "VALUES (:s, :m, :st) RETURNING terminalid");
    q.bindValue(":s", serial);
    q.bindValue(":m", m_modelId);
    q.bindValue(":st", status);
    if (!q.exec() || !q.next()) {
        m_lastError = q.lastError().text();
        return -1;
    }
    return q.value(0).toInt();
}

int TestConcurrency::seedSim(const QString& number)
{
    QSqlQuery q(m_testDb);
    q.prepare("INSERT INTO tblsimcards (simnumber) VALUES (:n) RETURNING simcardid");
    q.bindValue(":n", number);
    if (!q.exec() || !q.next()) {
        m_lastError = q.lastError().text();
        return -1;
    }
    return q.value(0).toInt();
}

int TestConcurrency::seedUser(const QString& username)
{
    QSqlQuery q(m_testDb);
    q.prepare("INSERT INTO tbl_users (username, role, is_active) "
              "VALUES (:u, 'user', TRUE) RETURNING user_id");
    q.bindValue(":u", username);
    if (!q.exec() || !q.next()) {
        m_lastError = q.lastError().text();
        return -1;
    }
    return q.value(0).toInt();
}

void TestConcurrency::resetTerminal(int terminalId, int simId)
{
    QSqlQuery t(m_testDb);
    t.prepare("UPDATE tblterminals SET status = 0, currentsimcardid = NULL WHERE terminalid = :id");
    t.bindValue(":id", terminalId);
    QVERIFY2(t.exec(), qPrintable(t.lastError().text()));

    QSqlQuery s(m_testDb);
    s.prepare("UPDATE tblsimcards SET status = 0 WHERE simcardid = :id");
    s.bindValue(":id", simId);
    QVERIFY2(s.exec(), qPrintable(s.lastError().text()));
}

QTEST_GUILESS_MAIN(TestConcurrency)
#include "test_concurrency.moc"