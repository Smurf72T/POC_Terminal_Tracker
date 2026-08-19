#include "database/repositories/terminalrepository.h"

#include <QHash>
#include <QSqlQuery>
#include <QStringList>
#include <QVariantList>

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
                   "COALESCE(NULLIF(s.simnumber, '') || CASE WHEN s2.simnumber IS NOT NULL THEN '; ' || s2.simnumber ELSE '' END, 'SIM не назначена') AS simstatus "
                   "FROM tblterminals t "
                   "LEFT JOIN tblmodels m ON t.modelid = m.modelid "
                   "LEFT JOIN tblsimcards s ON t.currentsimcardid = s.simcardid "
                   "LEFT JOIN tblsimcards s2 ON t.currentsimcardid2 = s2.simcardid "
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

models::Terminal TerminalRepository::loadById(int terminalId) const
{
    QSqlQuery query = makeQuery();
    query.prepare("SELECT t.terminalid, t.serialnumber, t.modelid, COALESCE(m.modelname, ''), "
                  "t.imei1, t.imei2, t.status, t.is_deactivated, t.currentsimcardid, "
                  "t.currentsimcardid2, t.purchasedate, t.notes, t.was_repaired "
                  "FROM tblterminals t "
                  "LEFT JOIN tblmodels m ON t.modelid = m.modelid "
                  "WHERE t.terminalid = :id");
    query.bindValue(":id", terminalId);
    if (!query.exec() || !query.next())
        return {};
    return makeTerminal(query, 0);
}

models::Terminal TerminalRepository::makeTerminal(const QSqlQuery& query, int startColumn) const
{
    models::Terminal t;
    t.id = query.value(startColumn + 0).toInt();
    t.serialNumber = query.value(startColumn + 1).toString();
    t.modelId = query.value(startColumn + 2).toInt();
    t.modelName = query.value(startColumn + 3).toString();
    t.imei1 = query.value(startColumn + 4).toString();
    t.imei2 = query.value(startColumn + 5).toString();
    t.status = query.value(startColumn + 6).toInt();
    t.deactivated = query.value(startColumn + 7).toBool();
    t.currentSimCardId = query.value(startColumn + 8).toInt();
    t.currentSimCard2Id = query.value(startColumn + 9).toInt();
    t.purchaseDate = query.value(startColumn + 10).toDate();
    t.notes = query.value(startColumn + 11).toString();
    t.wasRepaired = query.value(startColumn + 12).toBool();
    return t;
}

QVector<models::Terminal> TerminalRepository::loadByIds(const QList<int>& ids) const
{
    QVector<models::Terminal> result;
    if (ids.isEmpty())
        return result;
    QStringList placeholders;
    QVariantList values;
    for (int i = 0; i < ids.size(); ++i) {
        placeholders << QString(":id%1").arg(i);
        values << ids.at(i);
    }
    QSqlQuery query = makeQuery();
    query.prepare(QString("SELECT t.terminalid, t.serialnumber, t.modelid, COALESCE(m.modelname, ''), "
                          "t.imei1, t.imei2, t.status, t.is_deactivated, t.currentsimcardid, "
                          "t.currentsimcardid2, t.purchasedate, t.notes, t.was_repaired "
                          "FROM tblterminals t "
                          "LEFT JOIN tblmodels m ON t.modelid = m.modelid "
                          "WHERE t.terminalid IN (%1) ")
                      .arg(placeholders.join(", ")));
    for (int i = 0; i < ids.size(); ++i)
        query.bindValue(placeholders.at(i), values.at(i));
    if (!query.exec())
        return result;

    QHash<int, models::Terminal> byId;
    while (query.next()) {
        models::Terminal t = makeTerminal(query, 0);
        byId.insert(t.id, t);
    }
    for (int id : ids) {
        if (byId.contains(id))
            result.append(byId.value(id));
    }
    return result;
}

QVector<models::Terminal> TerminalRepository::loadFreeForSelection() const
{
    QVector<models::Terminal> result;
    QSqlQuery query = makeQuery();
    if (query.exec("SELECT t.terminalid, t.serialnumber, t.modelid, COALESCE(m.modelname, ''), "
                   "t.imei1, t.imei2, t.status, t.is_deactivated, t.currentsimcardid, "
                   "t.currentsimcardid2, t.purchasedate, t.notes, t.was_repaired "
                   "FROM tblterminals t "
                   "LEFT JOIN tblmodels m ON t.modelid = m.modelid "
                   "WHERE t.status = 0 AND t.is_deactivated = FALSE "
                   "ORDER BY t.serialnumber")) {
        while (query.next())
            result.append(makeTerminal(query, 0));
    }
    return result;
}

bool TerminalRepository::update(int terminalId, const TerminalUpdate& data) const
{
    QSqlQuery query = makeQuery();
    if (data.purchaseDate.isValid()) {
        query.prepare("UPDATE tblterminals SET serialnumber = :sn, modelid = :mid, "
                      "imei1 = :imei1, imei2 = :imei2, status = :status, "
                      "purchasedate = :pdate, notes = :notes, "
                      "was_repaired = :repaired, is_deactivated = :deactivated "
                      "WHERE terminalid = :id");
        query.bindValue(":pdate", data.purchaseDate);
    } else {
        query.prepare("UPDATE tblterminals SET serialnumber = :sn, modelid = :mid, "
                      "imei1 = :imei1, imei2 = :imei2, status = :status, "
                      "purchasedate = NULL, notes = :notes, "
                      "was_repaired = :repaired, is_deactivated = :deactivated "
                      "WHERE terminalid = :id");
    }
    query.bindValue(":sn", data.serialNumber.trimmed());
    query.bindValue(":mid", data.modelId);
    query.bindValue(":imei1", data.imei1);
    query.bindValue(":imei2", data.imei2);
    query.bindValue(":status", data.status);
    query.bindValue(":notes", data.notes);
    query.bindValue(":repaired", data.wasRepaired);
    query.bindValue(":deactivated", data.deactivated);
    query.bindValue(":id", terminalId);
    return query.exec();
}

void TerminalRepository::populateFreeTerminals(QSqlQueryModel* model) const
{
    QString queryStr = "SELECT t.serialnumber, m.modelname, "
                       "COALESCE(NULLIF(s.simnumber, '') || CASE WHEN s2.simnumber IS NOT NULL THEN '; ' || s2.simnumber ELSE '' END, 'SIM не назначена') AS simstatus "
                       "FROM tblterminals t "
                       "LEFT JOIN tblmodels m ON t.modelid = m.modelid "
                       "LEFT JOIN tblsimcards s ON t.currentsimcardid = s.simcardid "
                       "LEFT JOIN tblsimcards s2 ON t.currentsimcardid2 = s2.simcardid "
                       "WHERE t.status = 0 "
                       "ORDER BY t.serialnumber";
    model->setQuery(queryStr, m_db);
}