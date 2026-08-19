#ifndef TEST_DB_INTEGRATION_H
#define TEST_DB_INTEGRATION_H

#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>

// Интеграционные тесты против PostgreSQL (см. test_db_integration*.cpp).

QMap<QString, QString> loadEnvFile(const QString& filePath);
QString envValue(const QMap<QString, QString>& env, const QJsonObject& cfg, const QString& envKey,
                 const QString& cfgKey, const QString& def);
bool openConnection(QSqlDatabase& db, const QString& dbName, const QMap<QString, QString>& env,
                    const QJsonObject& cfg, QString* err);

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

#endif // TEST_DB_INTEGRATION_H