#include "databasemanager.h"
#include "utils/logging.h"
#include <QSqlQuery>
#include <QString>

void DatabaseManager::setCurrentUser(const QString& username)
{
    m_currentUser = username;
}

void DatabaseManager::setAuditUsername(const QString& username)
{
    // set_config(..., false) — параметр живёт до конца сессии.
    // Триггеры аудита (tblterminals/tblclients) читают current_setting('app.username').
    QSqlQuery query(m_database);
    query.prepare("SELECT set_config('app.username', :uname, false)");
    query.bindValue(":uname", username);
    if (!query.exec()) {
        qCWarning(logAudit) << "Не удалось установить app.username:" << query.lastError().text();
    }
}

void DatabaseManager::setSessionRole(const QString& role)
{
    // set_config(..., false) — параметр живёт до конца сессии.
    // Триггеры авторизации (tbl_users, tbl_audit_log) читают current_setting('app.role').
    QSqlQuery query(m_database);
    query.prepare("SELECT set_config('app.role', :role, false)");
    query.bindValue(":role", role);
    if (!query.exec()) {
        qCWarning(logAudit) << "Не удалось установить app.role:" << query.lastError().text();
    }
}

QString DatabaseManager::getCurrentUser() const
{
    return m_currentUser;
}

void DatabaseManager::setCurrentUserRole(const QString& role)
{
    m_currentUserRole = role;
}

QString DatabaseManager::getCurrentUserRole() const
{
    return m_currentUserRole;
}

bool DatabaseManager::isCurrentUserAdmin() const
{
    return m_currentUserRole == "admin";
}
