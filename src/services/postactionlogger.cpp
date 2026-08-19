#include "services/postactionlogger.h"
#include "database/databasemanager.h"

void PostActionLogger::log(const QString& action, const QString& tableName, int recordId)
{
    DatabaseManager::instance().logAction(action, tableName, recordId);
}

void PostActionLogger::notify()
{
    DatabaseManager::instance().notifyDataChanged();
}
