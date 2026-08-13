#include "database/repositories/simcardrepository.h"

#include <QSqlQuery>

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
                    "    WHERE t.currentsimcardid = s.simcardid "
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
                    "ORDER BY s.simnumber",
                    m_db);
}