// Stub для тестов — заглушка DatabaseManager через интерфейс IDatabaseManager.
// Реализует интерфейс вместо переопределения QObject-класса, поэтому
// #include "moc_databasemanager.cpp" больше не нужен.
#include "database/idatabasemanager.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QJsonObject>
#include <QStringList>
#include <functional>

class StubDatabaseManager : public IDatabaseManager
{
public:
    bool initialize(const QString &) override { return true; }
    bool isConnected() const override { return false; }
    void close() override {}
    void listenForDataChanges() override {}

    bool runMigrations(const QString &) override { return true; }
    QStringList pendingMigrations() override { return {}; }
    QSqlDatabase &getDatabase() override
    {
        static QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "stub");
        return db;
    }
    QJsonObject configObject() const override { return {}; }
    QSqlQuery executeQuery(const QString &, bool) override { return QSqlQuery(getDatabase()); }
    bool executeTransaction(const std::function<bool(QSqlDatabase &)> &) override { return true; }
    QString generateDocNumber(const QString &) override { return "DOC-2026-000001"; }
    void logAction(const QString &, const QString &, int, const QString &, const QString &, const QString &) override {}

    void notifyDataChanged() override {}
    void setCurrentUser(const QString &) override {}
    void setAuditUsername(const QString &) override {}
    void setSessionRole(const QString &) override {}
    QString getCurrentUser() const override { return "test"; }
    void setCurrentUserRole(const QString &) override {}
    QString getCurrentUserRole() const override { return "admin"; }
    bool isCurrentUserAdmin() const override { return true; }
};

IDatabaseManager &databaseManager()
{
    static StubDatabaseManager inst;
    return inst;
}
