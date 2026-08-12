#ifndef IDATABASEMANAGER_H
#define IDATABASEMANAGER_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <functional>

// Абстрактный интерфейс доступа к базе данных.
// Позволяет тестам подменять реальный DatabaseManager заглушкой без
// переопределения QObject-класса (раньше для этого использовался
// #include "moc_databasemanager.cpp" в tests/stub_databasemanager.cpp).
class IDatabaseManager
{
public:
    virtual ~IDatabaseManager() = default;

    virtual bool initialize(const QString &configPath) = 0;
    virtual bool isConnected() const = 0;
    virtual void close() = 0;
    virtual void listenForDataChanges() = 0;

    // Миграции БД
    virtual bool runMigrations(const QString &migrationsDir) = 0;
    virtual QStringList pendingMigrations() = 0;
    virtual const QSqlDatabase &getDatabase() const = 0;
    virtual QJsonObject configObject() const = 0;
    virtual QSqlQuery executeQuery(const QString &query, bool showErrorMessage) = 0;
    virtual bool executeTransaction(const std::function<bool(QSqlDatabase &)> &transactionFunc) = 0;
    virtual QString generateDocNumber(const QString &docType) = 0;
    virtual void logAction(const QString &action, const QString &tableName, int recordId,
                           const QString &username, const QString &oldValues,
                           const QString &newValues) = 0;

    virtual void notifyDataChanged() = 0;
    virtual void setCurrentUser(const QString &username) = 0;
    virtual void setAuditUsername(const QString &username) = 0;
    virtual void setSessionRole(const QString &role) = 0;
    virtual QString getCurrentUser() const = 0;
    virtual void setCurrentUserRole(const QString &role) = 0;
    virtual QString getCurrentUserRole() const = 0;
    virtual bool isCurrentUserAdmin() const = 0;
};

// Глобальная точка доступа. В приложении возвращает DatabaseManager::instance(),
// в тестах (tests/stub_databasemanager.cpp) — экземпляр-заглушку.
IDatabaseManager &databaseManager();

#endif // IDATABASEMANAGER_H
