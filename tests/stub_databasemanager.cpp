// Stub для тестов — минимальная реализация DatabaseManager
#include "databasemanager.h"
#include <QSqlDatabase>
#include <QJsonObject>
#include <functional>

DatabaseManager& DatabaseManager::instance()
{
    static DatabaseManager inst;
    return inst;
}

bool DatabaseManager::initialize(const QString&) { return true; }
bool DatabaseManager::isConnected() const { return false; }
void DatabaseManager::close() {}
bool DatabaseManager::runMigrations(const QString&) { return true; }
QStringList DatabaseManager::pendingMigrations() { return {}; }
QSqlDatabase& DatabaseManager::getDatabase() { static QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "stub"); return db; }
QSqlQuery DatabaseManager::executeQuery(const QString&, bool) { return QSqlQuery(getDatabase()); }
bool DatabaseManager::executeTransaction(const std::function<bool(QSqlDatabase&)>&) { return true; }
QString DatabaseManager::generateDocNumber(const QString&) { return "DOC-2026-000001"; }
void DatabaseManager::logAction(const QString&, const QString&, int, const QString&, const QString&, const QString&) {}
void DatabaseManager::notifyDataChanged() {}
void DatabaseManager::setCurrentUser(const QString&) {}
QString DatabaseManager::getCurrentUser() const { return "test"; }
void DatabaseManager::setCurrentUserRole(const QString&) {}
QString DatabaseManager::getCurrentUserRole() const { return "admin"; }
bool DatabaseManager::isCurrentUserAdmin() const { return true; }

bool DatabaseManager::loadConfig(const QString&) { return true; }
void DatabaseManager::showError(const QString&) {}
bool DatabaseManager::ensureMigrationsTable() { return true; }

#include "moc_databasemanager.cpp"
