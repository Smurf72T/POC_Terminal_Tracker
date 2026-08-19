#include "services/documentnumbergenerator.h"
#include "database/databasemanager.h"

QString DocumentNumberGenerator::generate(const QString& docType, QSqlDatabase& /*db*/)
{
    return DatabaseManager::instance().generateDocNumber(docType);
}
