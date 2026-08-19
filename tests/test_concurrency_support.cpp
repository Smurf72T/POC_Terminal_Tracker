#include "test_concurrency.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>

#include <thread>

QMap<QString, QString> loadEnvFile(const QString& filePath)
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

QString envValue(const QMap<QString, QString>& env, const QJsonObject& cfg, const QString& envKey,
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

bool openConnection(QSqlDatabase& db, const QString& dbName, const QMap<QString, QString>& env,
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

bool applyPendingWithLock(QSqlDatabase& db, const QString& migrationsDir)
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

void rentWorker(const QString& connName, const QString& dbName, const QMap<QString, QString>& env,
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

void batchWorker(const QString& connName, const QString& dbName, const QMap<QString, QString>& env,
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

void rateLimitWorker(const QString& connName, const QString& dbName, const QMap<QString, QString>& env,
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

void migrationWorker(const QString& connName, const QString& dbName, const QMap<QString, QString>& env,
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