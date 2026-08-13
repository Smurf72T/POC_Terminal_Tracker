#include "database/repositories/terminalrepository.h"

#include <QSqlQuery>

TerminalRepository::TerminalRepository(const QSqlDatabase& db) : m_db(db) {}

QSqlQuery TerminalRepository::makeQuery() const
{
    return QSqlQuery(m_db);
}

int TerminalRepository::countAll() const
{
    QSqlQuery query = makeQuery();
    if (!query.exec("SELECT COUNT(*) FROM tblterminals") || !query.next())
        return 0;
    return query.value(0).toInt();
}

int TerminalRepository::countByStatus(int status) const
{
    QSqlQuery query = makeQuery();
    query.prepare("SELECT COUNT(*) FROM tblterminals WHERE status = :status");
    query.bindValue(":status", status);
    if (!query.exec() || !query.next())
        return 0;
    return query.value(0).toInt();
}

QVector<TerminalRepository::StatusCount> TerminalRepository::statusCounts() const
{
    QVector<StatusCount> result;
    QSqlQuery query = makeQuery();
    if (query.exec("SELECT status, COUNT(*) FROM tblterminals GROUP BY status ORDER BY status")) {
        while (query.next())
            result.append({query.value(0).toInt(), query.value(1).toInt()});
    }
    return result;
}

QVector<TerminalRepository::FreeTerminal> TerminalRepository::loadFreeTerminals() const
{
    QVector<FreeTerminal> result;
    QSqlQuery query = makeQuery();
    if (query.exec("SELECT t.serialnumber, m.modelname, "
                   "COALESCE(s.simnumber, 'SIM не назначена') AS simstatus "
                   "FROM tblterminals t "
                   "LEFT JOIN tblmodels m ON t.modelid = m.modelid "
                   "LEFT JOIN tblsimcards s ON t.currentsimcardid = s.simcardid "
                   "WHERE t.status = 0 "
                   "ORDER BY t.serialnumber")) {
        while (query.next())
            result.append({query.value(0).toString(), query.value(1).toString(), query.value(2).toString()});
    }
    return result;
}

int TerminalRepository::findIdBySerial(const QString& serialNumber) const
{
    QSqlQuery query = makeQuery();
    query.prepare("SELECT terminalid FROM tblterminals WHERE serialnumber = :serial");
    query.bindValue(":serial", serialNumber);
    if (!query.exec() || !query.next())
        return -1;
    return query.value(0).toInt();
}

QVector<QPair<QString, int>> TerminalRepository::loadSerialsWithIds() const
{
    QVector<QPair<QString, int>> result;
    QSqlQuery query = makeQuery();
    if (query.exec("SELECT terminalid, serialnumber FROM tblterminals ORDER BY serialnumber")) {
        while (query.next())
            result.append({query.value(1).toString(), query.value(0).toInt()});
    }
    return result;
}

void TerminalRepository::populateFreeTerminals(QSqlQueryModel* model) const
{
    QString queryStr = "SELECT t.serialnumber, m.modelname, "
                       "COALESCE(s.simnumber, 'SIM не назначена') AS simstatus "
                       "FROM tblterminals t "
                       "LEFT JOIN tblmodels m ON t.modelid = m.modelid "
                       "LEFT JOIN tblsimcards s ON t.currentsimcardid = s.simcardid "
                       "WHERE t.status = 0 "
                       "ORDER BY t.serialnumber";
    model->setQuery(queryStr, m_db);
}