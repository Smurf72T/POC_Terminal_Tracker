#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QJsonObject>
#include <QStringList>

class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    static DatabaseManager& instance();

    bool initialize(const QString& configPath = "config/config.json");
    bool isConnected() const;
    void close();

    // Миграции БД
    bool runMigrations(const QString &migrationsDir = "sql/migrations/");
    QStringList pendingMigrations();

    QSqlDatabase& getDatabase();
    QSqlQuery executeQuery(const QString& query, bool showErrorMessage = true);
    bool executeTransaction(const std::function<bool(QSqlDatabase&)>& transactionFunc);
    QString generateDocNumber(const QString& docType); // receipt, rental, return, payment
    void logAction(const QString& action, const QString& tableName, int recordId,
                   const QString& username = "admin", const QString& oldValues = "{}",
                   const QString& newValues = "{}");

    void notifyDataChanged();
    void setCurrentUser(const QString& username);
    void setAuditUsername(const QString& username);
    QString getCurrentUser() const;
    void setCurrentUserRole(const QString& role);
    QString getCurrentUserRole() const;
    bool isCurrentUserAdmin() const;

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

    QSqlDatabase m_database;
    QJsonObject m_config;
    bool m_initialized = false;
    QString m_currentUser = "admin";
    QString m_currentUserRole = "admin";
};

#endif // DATABASEMANAGER_H