#include "database/repositories/simcardrepository.h"

#include <QHash>
#include <QSqlQuery>
#include <QStringList>
#include <QVariantList>

SimCardRepository::SimCardRepository(const QSqlDatabase& db) : m_db(db) {}

int SimCardRepository::countAll() const
{
    QSqlQuery query(m_db);
    if (!query.exec("SELECT COUNT(*) FROM tblsimcards") || !query.next())
        return 0;
    return query.value(0).toInt();
}

int SimCardRepository::countFree() const
{
    QSqlQuery query(m_db);
    if (!query.exec("SELECT COUNT(*) FROM tblsimcards s "
                    "WHERE s.status = 0 "
                    "OR EXISTS ("
                    "    SELECT 1 FROM tblterminals t "
                    "    WHERE (t.currentsimcardid = s.simcardid OR t.currentsimcardid2 = s.simcardid) "
                    "    AND t.status = 0"
                    ")") ||
        !query.next())
        return 0;
    return query.value(0).toInt();
}

QVector<SimCardRepository::FreeSimCard> SimCardRepository::loadFreeSimCards() const
{
    QVector<FreeSimCard> result;
    QSqlQuery query(m_db);
    if (!query.exec("SELECT s.simnumber, s.notes, 'Не используется' AS status "
                    "FROM tblsimcards s "
                    "WHERE s.status = 0 "
                    "AND s.simcardid NOT IN ("
                    "    SELECT t.currentsimcardid FROM tblterminals t WHERE t.currentsimcardid IS NOT NULL"
                    ") "
                    "AND s.simcardid NOT IN ("
                    "    SELECT t.currentsimcardid2 FROM tblterminals t WHERE t.currentsimcardid2 IS NOT NULL"
                    ") "
                    "ORDER BY s.simnumber"))
        return result;
    while (query.next()) {
        result.append({query.value(0).toString(), query.value(1).toString(), query.value(2).toString()});
    }
    return result;
}

void SimCardRepository::populateFreeSimCards(QSqlQueryModel* model) const
{
    model->setQuery("SELECT s.simnumber, s.notes, 'Не используется' AS status "
                    "FROM tblsimcards s "
                    "WHERE s.status = 0 "
                    "AND s.simcardid NOT IN ("
                    "    SELECT t.currentsimcardid FROM tblterminals t WHERE t.currentsimcardid IS NOT NULL"
                    ") "
                    "AND s.simcardid NOT IN ("
                    "    SELECT t.currentsimcardid2 FROM tblterminals t WHERE t.currentsimcardid2 IS NOT NULL"
                    ") "
                    "ORDER BY s.simnumber",
                    m_db);
}

models::SimCard SimCardRepository::loadById(int simCardId) const
{
    QSqlQuery query(m_db);
    query.prepare("SELECT simcardid, simnumber, status, notes, createdat FROM tblsimcards WHERE simcardid = :id");
    query.bindValue(":id", simCardId);
    if (!query.exec() || !query.next())
        return {};
    models::SimCard s;
    s.id = query.value(0).toInt();
    s.number = query.value(1).toString();
    s.status = query.value(2).toInt();
    s.notes = query.value(3).toString();
    s.createdAt = query.value(4).toDate();
    return s;
}

QVector<models::SimCard> SimCardRepository::loadByIds(const QList<int>& ids) const
{
    QVector<models::SimCard> result;
    if (ids.isEmpty())
        return result;
    QStringList placeholders;
    QVariantList values;
    for (int i = 0; i < ids.size(); ++i) {
        placeholders << QString(":id%1").arg(i);
        values << ids.at(i);
    }
    QSqlQuery query(m_db);
    query.prepare(QString("SELECT simcardid, simnumber, status, notes, createdat "
                          "FROM tblsimcards WHERE simcardid IN (%1) ")
                      .arg(placeholders.join(", ")));
    for (int i = 0; i < ids.size(); ++i)
        query.bindValue(placeholders.at(i), values.at(i));
    if (!query.exec())
        return result;

    QHash<int, models::SimCard> byId;
    while (query.next()) {
        models::SimCard s;
        s.id = query.value(0).toInt();
        s.number = query.value(1).toString();
        s.status = query.value(2).toInt();
        s.notes = query.value(3).toString();
        s.createdAt = query.value(4).toDate();
        byId.insert(s.id, s);
    }
    for (int id : ids) {
        if (byId.contains(id))
            result.append(byId.value(id));
    }
    return result;
}

QVector<models::SimCard> SimCardRepository::loadFreeForSelection() const
{
    QVector<models::SimCard> result;
    QSqlQuery query(m_db);
    if (query.exec("SELECT s.simcardid, s.simnumber "
                   "FROM tblsimcards s "
                   "WHERE s.status = 0 "
                   "OR EXISTS ("
                   "    SELECT 1 FROM tblterminals t "
                   "    WHERE (t.currentsimcardid = s.simcardid OR t.currentsimcardid2 = s.simcardid) "
                   "    AND t.status = 0"
                   ")"
                   "ORDER BY s.simnumber")) {
        while (query.next()) {
            models::SimCard s;
            s.id = query.value(0).toInt();
            s.number = query.value(1).toString();
            result.append(s);
        }
    }
    return result;
}