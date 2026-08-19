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
#include <QSet>
#include <QDateTime>
#include <QCoreApplication>

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

// Конкуренто-тесты: многопользовательский доступ к PostgreSQL.
// Каждый поток открывает СОБСТВЕННОЕ соединение (QSqlDatabase не потокобезопасна,
// Qt требует одно соединение на поток). Проверяются гонки, которые не ловятся
// одиночным соединением: выдача одной SIM двум пользователям, потерянные
// обновления статусов, генерация номеров, миграции при одновременном старте,
// rate limiting.

static const qint64 kMigrationLockKey = 0x504F434D494752; // "POCMIGR" (как в приложении)

class TestConcurrency : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void test_concurrent_doc_numbers();
    void test_concurrent_sim_assignment();
    void test_concurrent_rental_load();
    void test_concurrent_batch_vs_rental();
    void test_concurrent_rate_limit();
    void test_concurrent_migrations();

private:
    QSqlDatabase m_adminDb;
    QSqlDatabase m_testDb;
    QString m_testDbName = "pocbase_concurrency_test";
    QMap<QString, QString> m_env;
    QJsonObject m_dbConfig;
    int m_modelId = 0;
    QString m_lastError;

    bool applyMigrations();
    bool execOnTest(const QString& sql, QString* err = nullptr);
    QSqlQuery queryOnTest(const QString& sql, QString* err = nullptr);
    int countOnTest(const QString& sql);
    int seedTerminal(const QString& serial, int status);
    int seedSim(const QString& number);
    int seedUser(const QString& username);
    void resetTerminal(int terminalId, int simId);
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

// ---------- worker: атомарная попытка выдачи терминала с SIM (как RentalForm) ----------
// Возвращает true, только если транзакция прошла целиком: терминал заблокирован
// FOR UPDATE NOWAIT и свободен, SIM заблокирована и свободна, оба обновлены.
struct RentalAttempt {
    bool ok = false;
    QString error;
};

static void rentWorker(const QString& connName, const QString& dbName, const QMap<QString, QString>& env,
                       const QJsonObject& cfg, std::atomic<bool>* startGate, std::atomic<int>* successCount,
                       std::mutex* errorMutex, QString* firstError, int terminalId, int simId)
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL", connName);
    QString err;
    if (!openConnection(db, dbName, env, cfg, &err)) {
        {
            std::lock_guard<std::mutex> lk(*errorMutex);
            if (firstError->isEmpty())
                *firstError = "не удалось открыть соединение: " + err;
        }
        QSqlDatabase::removeDatabase(connName);
        return;
    }

    while (!startGate->load())
        std::this_thread::yield();

    RentalAttempt result;
    if (!db.transaction()) {
        result.error = "transaction: " + db.lastError().text();
    } else {
        QSqlQuery q(db);
        q.prepare("SELECT status FROM tblterminals WHERE terminalid = :id FOR UPDATE NOWAIT");
        q.bindValue(":id", terminalId);
        if (!q.exec() || !q.next()) {
            result.error = "терминал не заблокирован: " + db.lastError().text();
        } else if (q.value(0).toInt() != 0) {
            result.error = "терминал занят";
        } else {
            QSqlQuery simLock(db);
            simLock.prepare("SELECT status FROM tblsimcards WHERE simcardid = :id AND status = 0 FOR UPDATE NOWAIT");
            simLock.bindValue(":id", simId);
            if (!simLock.exec() || !simLock.next()) {
                result.error = "SIM не заблокирована: " + db.lastError().text();
            } else {
                QSqlQuery u1(db);
                u1.prepare("UPDATE tblterminals SET status = 1, currentsimcardid = :s WHERE terminalid = :t");
                u1.bindValue(":s", simId);
                u1.bindValue(":t", terminalId);
                QSqlQuery u2(db);
                u2.prepare("UPDATE tblsimcards SET status = 1 WHERE simcardid = :id");
                u2.bindValue(":id", simId);
                if (!u1.exec() || !u2.exec()) {
                    result.error = "update: " + db.lastError().text();
                } else if (db.commit()) {
                    result.ok = true;
                } else {
                    result.error = "commit: " + db.lastError().text();
                }
            }
        }
        if (!result.ok)
            db.rollback();
    }

    if (result.ok) {
        successCount->fetch_add(1);
    } else if (!result.error.isEmpty()) {
        std::lock_guard<std::mutex> lk(*errorMutex);
        if (firstError->isEmpty())
            *firstError = result.error;
    }

    db.close();
    QSqlDatabase::removeDatabase(connName);
}

// ---------- worker: единичное обновление статуса с предусловием (как BatchStatusForm) ----------
struct BatchAttempt {
    bool ok = false;
    QString error;
};

static void batchWorker(const QString& connName, const QString& dbName, const QMap<QString, QString>& env,
                        const QJsonObject& cfg, std::atomic<bool>* startGate, std::atomic<int>* successCount,
                        int terminalId, int expectedStatus, int newStatus)
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL", connName);
    if (!openConnection(db, dbName, env, cfg, nullptr)) {
        QSqlDatabase::removeDatabase(connName);
        return;
    }

    while (!startGate->load())
        std::this_thread::yield();

    bool ok = false;
    if (db.transaction()) {
        QSqlQuery lockQ(db);
        lockQ.prepare("SELECT status FROM tblterminals WHERE terminalid = :id FOR UPDATE NOWAIT");
        lockQ.bindValue(":id", terminalId);
        if (lockQ.exec() && lockQ.next()) {
            if (lockQ.value(0).toInt() == expectedStatus) {
                QSqlQuery up(db);
                up.prepare("UPDATE tblterminals SET status = :s WHERE terminalid = :id");
                up.bindValue(":s", newStatus);
                up.bindValue(":id", terminalId);
                if (up.exec() && db.commit())
                    ok = true;
            }
        }
        if (!ok)
            db.rollback();
    }

    if (ok)
        successCount->fetch_add(1);

    db.close();
    QSqlDatabase::removeDatabase(connName);
}

// ---------- worker: неудачная попытка входа (атомарный UPDATE, как в LoginForm) ----------
struct RateLimitResult {
    int rowsReturned = 0;
    int maxAttempts = 0;
};

static void rateLimitWorker(const QString& connName, const QString& dbName, const QMap<QString, QString>& env,
                            const QJsonObject& cfg, std::atomic<bool>* startGate, RateLimitResult* out,
                            const QString& username)
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL", connName);
    if (!openConnection(db, dbName, env, cfg, nullptr)) {
        QSqlDatabase::removeDatabase(connName);
        return;
    }

    while (!startGate->load())
        std::this_thread::yield();

    {
        QSqlQuery upd(db);
        upd.prepare("UPDATE tbl_users SET "
                    "failed_login_attempts = CASE WHEN locked_until IS NOT NULL "
                    "    THEN 1 ELSE failed_login_attempts + 1 END, "
                    "locked_until = CASE WHEN "
                    "    (CASE WHEN locked_until IS NOT NULL "
                    "         THEN 1 ELSE failed_login_attempts + 1 END) >= 5 "
                    "    THEN NOW() + INTERVAL '30 seconds' ELSE NULL END "
                    "WHERE username = :uname "
                    "  AND (locked_until IS NULL OR locked_until <= NOW()) "
                    "RETURNING failed_login_attempts");
        upd.bindValue(":uname", username);

        if (upd.exec()) {
            while (upd.next()) {
                out->rowsReturned++;
                out->maxAttempts = qMax(out->maxAttempts, upd.value(0).toInt());
            }
        }
    }

    db.close();
    QSqlDatabase::removeDatabase(connName);
}

// ---------- worker: применение миграций под advisory lock (как runMigrations) ----------
static bool applyPendingWithLock(QSqlDatabase& db, const QString& migrationsDir)
{
    QSqlQuery lockQ(db);
    lockQ.prepare("SELECT pg_advisory_lock(:key)");
    lockQ.bindValue(":key", kMigrationLockKey);
    if (!lockQ.exec())
        return false;

    bool ok = true;
    QStringList pending;
    {
        QSet<QString> applied;
        QSqlQuery q(db);
        if (q.exec("SELECT version FROM schema_migrations")) {
            while (q.next())
                applied.insert(q.value(0).toString());
        }
        QDirIterator it(migrationsDir, QStringList() << "*.sql", QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            if (!applied.contains(it.fileName()))
                pending.append(it.filePath());
        }
        pending.sort();
    }

    for (const QString& filePath : pending) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            ok = false;
            break;
        }
        QString sql = QString::fromUtf8(file.readAll());
        file.close();

        if (!db.transaction()) {
            ok = false;
            break;
        }
        QSqlQuery q(db);
        if (!q.exec(sql)) {
            db.rollback();
            ok = false;
            break;
        }
        QSqlQuery rec(db);
        rec.prepare("INSERT INTO schema_migrations (version) VALUES (:v)");
        rec.bindValue(":v", QFileInfo(filePath).fileName());
        if (!rec.exec() || !db.commit()) {
            db.rollback();
            ok = false;
            break;
        }
    }

    QSqlQuery unlockQ(db);
    unlockQ.prepare("SELECT pg_advisory_unlock(:key)");
    unlockQ.bindValue(":key", kMigrationLockKey);
    unlockQ.exec();
    return ok;
}

static void migrationWorker(const QString& connName, const QString& dbName, const QMap<QString, QString>& env,
                            const QJsonObject& cfg, std::atomic<bool>* startGate, const QString& migrationsDir,
                            std::atomic<int>* okCount)
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL", connName);
    if (!openConnection(db, dbName, env, cfg, nullptr)) {
        QSqlDatabase::removeDatabase(connName);
        return;
    }

    while (!startGate->load())
        std::this_thread::yield();

    if (applyPendingWithLock(db, migrationsDir))
        okCount->fetch_add(1);

    db.close();
    QSqlDatabase::removeDatabase(connName);
}

// ==================== TestCase ====================

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

void TestConcurrency::test_concurrent_doc_numbers()
{
    const int threadCount = 8;
    const int numbersPerThread = 50;
    const QString connPrefix = "numConn";

    std::atomic<bool> startGate(false);
    QSet<QString> allNumbers;
    std::mutex setMutex;

    std::vector<std::thread> threads;
    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([this, i, &startGate, &allNumbers, &setMutex, &connPrefix]() {
            QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL", connPrefix + QString::number(i));
            if (!openConnection(db, m_testDbName, m_env, m_dbConfig, nullptr)) {
                QSqlDatabase::removeDatabase(connPrefix + QString::number(i));
                return;
            }
            while (!startGate.load())
                std::this_thread::yield();

            for (int k = 0; k < numbersPerThread; ++k) {
                QSqlQuery q(db);
                q.prepare("SELECT generate_doc_number(:t)");
                q.bindValue(":t", "receipt");
                if (q.exec() && q.next()) {
                    std::lock_guard<std::mutex> lk(setMutex);
                    allNumbers.insert(q.value(0).toString());
                }
            }

            db.close();
            QSqlDatabase::removeDatabase(connPrefix + QString::number(i));
        });
    }

    startGate.store(true);
    for (auto& th : threads)
        th.join();

    QCOMPARE(allNumbers.size(), qsizetype(threadCount * numbersPerThread));
}

void TestConcurrency::test_concurrent_sim_assignment()
{
    const int threadCount = 8;
    int terminalId = seedTerminal("КОНК-СИМ-ТЕРМ-001", 0);
    QVERIFY2(terminalId != -1, qPrintable(m_lastError));
    int simId = seedSim("КОНК-СИМ-001");
    QVERIFY2(simId != -1, qPrintable(m_lastError));

    std::atomic<bool> startGate(false);
    std::atomic<int> successCount(0);
    std::mutex errorMutex;
    QString firstError;

    std::vector<std::thread> threads;
    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([&, i]() {
            rentWorker("simConn" + QString::number(i), m_testDbName, m_env, m_dbConfig, &startGate, &successCount,
                       &errorMutex, &firstError, terminalId, simId);
        });
    }

    startGate.store(true);
    for (auto& th : threads)
        th.join();

    QCOMPARE(successCount.load(), 1);
    QVERIFY2(!firstError.isEmpty(), qPrintable("ожидались конкурентные отказы"));

    QCOMPARE(countOnTest("SELECT status FROM tblterminals WHERE terminalid = " + QString::number(terminalId)), 1);
    QCOMPARE(countOnTest("SELECT currentsimcardid FROM tblterminals WHERE terminalid = " + QString::number(terminalId)),
             simId);
    QCOMPARE(countOnTest("SELECT status FROM tblsimcards WHERE simcardid = " + QString::number(simId)), 1);

    resetTerminal(terminalId, simId);
}

void TestConcurrency::test_concurrent_rental_load()
{
    const int threadCount = 8;
    std::vector<int> terminalIds;
    std::vector<int> simIds;
    for (int i = 0; i < threadCount; ++i) {
        int tid = seedTerminal("КОНК-НАГРУЗКА-" + QString::number(i), 0);
        QVERIFY2(tid != -1, qPrintable(m_lastError));
        int sid = seedSim("КОНК-SIM-НАГР-" + QString::number(i));
        QVERIFY2(sid != -1, qPrintable(m_lastError));
        terminalIds.push_back(tid);
        simIds.push_back(sid);
    }

    std::atomic<bool> startGate(false);
    std::atomic<int> successCount(0);
    std::mutex errorMutex;
    QString firstError;

    std::vector<std::thread> threads;
    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([&, i]() {
            rentWorker("loadConn" + QString::number(i), m_testDbName, m_env, m_dbConfig, &startGate, &successCount,
                       &errorMutex, &firstError, terminalIds[i], simIds[i]);
        });
    }

    startGate.store(true);
    for (auto& th : threads)
        th.join();

    QCOMPARE(successCount.load(), threadCount);
    QVERIFY2(firstError.isEmpty(), qPrintable("неожиданная ошибка: " + firstError));

    QCOMPARE(countOnTest("SELECT count(*) FROM tblterminals WHERE status = 1"), threadCount);
    QCOMPARE(countOnTest("SELECT count(*) FROM tblsimcards WHERE status = 1"), threadCount);
    QCOMPARE(countOnTest("SELECT count(DISTINCT currentsimcardid) FROM tblterminals "
                         "WHERE currentsimcardid IS NOT NULL"),
             threadCount);
}

void TestConcurrency::test_concurrent_batch_vs_rental()
{
    for (int round = 0; round < 4; ++round) {
        int terminalId = seedTerminal("КОНК-ГОНКА-" + QString::number(round), 0);
        QVERIFY2(terminalId != -1, qPrintable(m_lastError));
        int simId = seedSim("КОНК-SIM-ГОНКА-" + QString::number(round));
        QVERIFY2(simId != -1, qPrintable(m_lastError));

        std::atomic<bool> startGate(false);
        std::atomic<int> rentSuccess(0);
        std::atomic<int> batchSuccess(0);
        std::mutex errorMutex;
        QString firstError;

        std::thread t1([&]() {
            rentWorker("raceRentConn", m_testDbName, m_env, m_dbConfig, &startGate, &rentSuccess, &errorMutex,
                       &firstError, terminalId, simId);
        });
        std::thread t2([&]() {
            batchWorker("raceBatchConn", m_testDbName, m_env, m_dbConfig, &startGate, &batchSuccess, terminalId, 0, 1);
        });

        startGate.store(true);
        t1.join();
        t2.join();

        QCOMPARE(rentSuccess.load() + batchSuccess.load(), 1);

        // Согласованность финального состояния: терминал всегда «в аренде» (статус 1),
        // при этом либо аренда (SIM привязана и занята), либо массовая смена (SIM свободна)
        QCOMPARE(countOnTest("SELECT status FROM tblterminals WHERE terminalid = " + QString::number(terminalId)), 1);
        int simStatus = countOnTest("SELECT status FROM tblsimcards WHERE simcardid = " + QString::number(simId));
        int currentSim =
            countOnTest("SELECT currentsimcardid FROM tblterminals WHERE terminalid = " + QString::number(terminalId));
        if (rentSuccess.load() == 1) {
            QCOMPARE(currentSim, simId);
            QCOMPARE(simStatus, 1);
        } else {
            QCOMPARE(currentSim, 0);
            QCOMPARE(simStatus, 0);
        }
    }
}

void TestConcurrency::test_concurrent_rate_limit()
{
    const QString username = "conclimit";
    execOnTest("DELETE FROM tbl_users WHERE username = '" + username + "'");
    QVERIFY2(seedUser(username) != -1, qPrintable(m_lastError));

    const int threadCount = 20;
    std::atomic<bool> startGate(false);
    std::vector<RateLimitResult> results(threadCount);

    std::vector<std::thread> threads;
    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([&, i]() {
            rateLimitWorker("rateConn" + QString::number(i), m_testDbName, m_env, m_dbConfig, &startGate, &results[i],
                            username);
        });
    }

    startGate.store(true);
    for (auto& th : threads)
        th.join();

    int totalReturned = 0;
    int maxAttempts = 0;
    for (const RateLimitResult& r : results) {
        totalReturned += r.rowsReturned;
        maxAttempts = qMax(maxAttempts, r.maxAttempts);
    }

    // Ровно 5 попыток пробивают до активации блокировки, остальные отклонены WHERE-условием
    QCOMPARE(totalReturned, 5);
    QCOMPARE(maxAttempts, 5);

    QCOMPARE(countOnTest("SELECT failed_login_attempts FROM tbl_users WHERE username = '" + username + "'"), 5);
    QCOMPARE(
        countOnTest("SELECT count(*) FROM tbl_users WHERE username = '" + username + "' AND locked_until IS NOT NULL"),
        1);

    execOnTest("UPDATE tbl_users SET failed_login_attempts = 0, locked_until = NULL "
               "WHERE username = '" +
               username + "'");
}

void TestConcurrency::test_concurrent_migrations()
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
    QVERIFY2(!migrationsDir.isEmpty(), "Директория миграций не найдена");

    // Имитация одновременного старта: два последних файла снова «не применены»
    QSqlQuery del(m_testDb);
    QVERIFY2(del.exec("DELETE FROM schema_migrations "
                      "WHERE version IN ('006_terminal_status_check.sql', "
                      "                   '007_data_change_notify.sql')"),
             qPrintable(del.lastError().text()));

    std::atomic<bool> startGate(false);
    std::atomic<int> okCount(0);

    std::thread t1(
        [&]() { migrationWorker("migConn1", m_testDbName, m_env, m_dbConfig, &startGate, migrationsDir, &okCount); });
    std::thread t2(
        [&]() { migrationWorker("migConn2", m_testDbName, m_env, m_dbConfig, &startGate, migrationsDir, &okCount); });

    startGate.store(true);
    t1.join();
    t2.join();

    QCOMPARE(okCount.load(), 2);
    QCOMPARE(countOnTest("SELECT count(*) FROM schema_migrations"), 14);

    // Триггер уведомлений на месте и функция определена
    QCOMPARE(countOnTest("SELECT count(*) FROM pg_trigger "
                         "WHERE tgname = 'trg_data_change_notify_tblterminals'"),
             1);
    QCOMPARE(countOnTest("SELECT count(*) FROM pg_proc WHERE proname = 'notify_data_changed'"), 1);
}

QTEST_GUILESS_MAIN(TestConcurrency)
#include "test_concurrency.moc"
