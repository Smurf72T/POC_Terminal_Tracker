#ifndef TEST_CONCURRENCY_H
#define TEST_CONCURRENCY_H

#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>

#include <atomic>
#include <mutex>

// Конкурентные тесты разделены на несколько TU (test_concurrency*.cpp) —
// реализация общая для всех: заголовок описывает класс и работников потока.

struct RentalAttempt {
    bool ok = false;
    QString error;
};

struct BatchAttempt {
    bool ok = false;
    QString error;
};

struct RateLimitResult {
    int rowsReturned = 0;
    int maxAttempts = 0;
};

// "POCMIGR" (как в приложении)
inline constexpr qint64 kMigrationLockKey = 0x504F434D494752;

QMap<QString, QString> loadEnvFile(const QString& filePath);
QString envValue(const QMap<QString, QString>& env, const QJsonObject& cfg, const QString& envKey,
                 const QString& cfgKey, const QString& def);
bool openConnection(QSqlDatabase& db, const QString& dbName, const QMap<QString, QString>& env,
                    const QJsonObject& cfg, QString* err);

// Работники: применяют миграции под advisory lock (как runMigrations).
bool applyPendingWithLock(QSqlDatabase& db, const QString& migrationsDir);

// ---------- worker: атомарная попытка выдачи терминала с SIM (как RentalForm) ----------
// Возвращает true, только если транзакция прошла целиком: терминал заблокирован
// FOR UPDATE NOWAIT и свободен, SIM заблокирована и свободна, оба обновлены.
void rentWorker(const QString& connName, const QString& dbName, const QMap<QString, QString>& env,
                const QJsonObject& cfg, std::atomic<bool>* startGate, std::atomic<int>* successCount,
                std::mutex* errorMutex, QString* firstError, int terminalId, int simId);

// ---------- worker: единичное обновление статуса с предусловием (как BatchStatusForm) ----------
void batchWorker(const QString& connName, const QString& dbName, const QMap<QString, QString>& env,
                 const QJsonObject& cfg, std::atomic<bool>* startGate, std::atomic<int>* successCount,
                 int terminalId, int expectedStatus, int newStatus);

// ---------- worker: неудачная попытка входа (атомарный UPDATE, как в LoginForm) ----------
void rateLimitWorker(const QString& connName, const QString& dbName, const QMap<QString, QString>& env,
                     const QJsonObject& cfg, std::atomic<bool>* startGate, RateLimitResult* out,
                     const QString& username);

void migrationWorker(const QString& connName, const QString& dbName, const QMap<QString, QString>& env,
                     const QJsonObject& cfg, std::atomic<bool>* startGate, const QString& migrationsDir,
                     std::atomic<int>* okCount);

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

#endif // TEST_CONCURRENCY_H