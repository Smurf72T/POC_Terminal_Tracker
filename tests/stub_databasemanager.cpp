// Stub для тестов — заглушка DatabaseManager через интерфейс IDatabaseManager.
// Реализует интерфейс вместо переопределения QObject-класса, поэтому
// #include "moc_databasemanager.cpp" больше не нужен.
#include "database/idatabasemanager.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QJsonObject>
#include <QStringList>
#include <functional>

class StubDatabaseManager : public IDatabaseManager {
public:
    bool initialize(const QString&) override { return true; }
    bool isConnected() const override { return false; }
    void close() override {}
    void listenForDataChanges() override {}

    bool runMigrations(const QString&) override { return true; }
    QStringList pendingMigrations() override { return {}; }
    const QSqlDatabase& getDatabase() const override
    {
        static QSqlDatabase db = []() {
            QSqlDatabase d = QSqlDatabase::addDatabase("QSQLITE", "stub");
            d.setDatabaseName(":memory:");
            if (!d.open())
                return d;
            QSqlQuery q(d);
            q.exec("CREATE TABLE tblterminals (terminalid INTEGER PRIMARY KEY AUTOINCREMENT, "
                   "serialnumber TEXT UNIQUE, imei1 TEXT, imei2 TEXT)");
            q.prepare("INSERT INTO tblterminals (serialnumber, imei1, imei2) VALUES (?, ?, ?)");
            q.addBindValue("SN-ALREADY-0001");
            q.addBindValue("111111111111111");
            q.addBindValue("222222222222222");
            q.exec();
            q.prepare("INSERT INTO tblterminals (serialnumber, imei1, imei2) VALUES (?, ?, ?)");
            q.addBindValue("SN-ALREADY-0002");
            q.addBindValue("333333333333333");
            q.addBindValue(QString());
            q.exec();
            return d;
        }();
        return db;
    }
    QJsonObject configObject() const override { return {}; }
    QSqlQuery executeQuery(const QString&, bool) override { return QSqlQuery(getDatabase()); }
    bool executeTransaction(const std::function<bool(QSqlDatabase&)>&) override { return true; }
    QString generateDocNumber(const QString&) override { return "DOC-2026-000001"; }
    void logAction(const QString&, const QString&, int, const QString&, const QString&, const QString&) override {}

    void notifyDataChanged() override {}
    void setCurrentUser(const QString&) override {}
    void setAuditUsername(const QString&) override {}
    void setSessionRole(const QString&) override {}
    QString getCurrentUser() const override { return "test"; }
    void setCurrentUserRole(const QString&) override {}
    QString getCurrentUserRole() const override { return "admin"; }
    bool isCurrentUserAdmin() const override { return true; }
};

IDatabaseManager& databaseManager()
{
    static StubDatabaseManager inst;
    return inst;
}
