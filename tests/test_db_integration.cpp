#include "test_db_integration.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSqlError>
#include <QtTest>

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

QTEST_GUILESS_MAIN(TestDbIntegration)
#include "test_db_integration.moc"