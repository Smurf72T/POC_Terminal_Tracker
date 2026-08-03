#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QJsonObject>
#include <QStringList>

#include "idatabasemanager.h"
#include "utils/circuitbreaker.h"

class DatabaseManager : public QObject, public IDatabaseManager
{
    Q_OBJECT

public:
    static DatabaseManager& instance();

    static void setSuppressDialogs(bool suppress);
    static bool suppressDialogs();

    bool initialize(const QString& configPath = "config/config.json") override;
    bool isConnected() const override;
    void close() override;
    void listenForDataChanges() override;

    // Миграции БД
    bool runMigrations(const QString &migrationsDir = "sql/migrations/") override;
    QStringList pendingMigrations() override;
    QSqlDatabase& getDatabase() override;
    QJsonObject configObject() const override;
    QSqlQuery executeQuery(const QString& query, bool showErrorMessage = true) override;
    bool executeTransaction(const std::function<bool(QSqlDatabase&)>& transactionFunc) override;
    QString generateDocNumber(const QString& docType) override; // receipt, rental, return, payment
    void logAction(const QString& action, const QString& tableName, int recordId,
                   const QString& username = QString(), const QString& oldValues = "{}",
                   const QString& newValues = "{}") override;

    void notifyDataChanged() override;
    void setCurrentUser(const QString& username) override;
    void setAuditUsername(const QString& username) override;
    void setSessionRole(const QString& role) override;
    QString getCurrentUser() const override;
    void setCurrentUserRole(const QString& role) override;
    QString getCurrentUserRole() const override;
    bool isCurrentUserAdmin() const override;

signals:
    void dataChanged();

private:
    DatabaseManager() = default;
    ~DatabaseManager() = default;
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    bool loadConfig(const QString& configPath);
    void showError(const QString& message);
    bool ensureMigrationsTable();
    bool applyPendingMigrations();

    QSqlDatabase m_database;
    QJsonObject m_config;
    bool m_initialized = false;
    bool m_listening = false;
    QString m_currentUser = "system";
    QString m_currentUserRole = "user";

    static bool s_suppressDialogs;
    CircuitBreaker m_circuitBreaker;
};

#endif // DATABASEMANAGER_H