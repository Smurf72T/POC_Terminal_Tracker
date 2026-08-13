#include "database/repositories/clientrepository.h"

#include <QSqlQuery>

#include <utility>

ClientRepository::ClientRepository(const QSqlDatabase& db) : m_db(db) {}

int ClientRepository::countAll() const
{
    QSqlQuery query(m_db);
    if (!query.exec("SELECT COUNT(*) FROM tblclients") || !query.next())
        return 0;
    return query.value(0).toInt();
}

QVector<ClientRepository::RentalStatistic> ClientRepository::loadRentalStatistics() const
{
    QVector<RentalStatistic> result;
    QSqlQuery query(m_db);
    if (!query.exec("SELECT c.clientid, c.clientname AS \"Клиент\", "
                    "COUNT(t.terminalid) AS \"Терминалов в аренде\" "
                    "FROM tblclients c "
                    "JOIN tblrentaldocs r ON c.clientid = r.clientid "
                    "JOIN tblrentaldetails rd ON r.rentaldocid = rd.rentaldocid "
                    "JOIN tblterminals t ON rd.terminalid = t.terminalid AND t.status = 1 "
                    "GROUP BY c.clientid, c.clientname "
                    "ORDER BY COUNT(t.terminalid) DESC"))
        return result;
    while (query.next()) {
        result.append({query.value(0).toInt(), query.value(1).toString(), query.value(2).toInt()});
    }
    return result;
}

void ClientRepository::populateRentalStatistics(QSqlQueryModel* model) const
{
    model->setQuery("SELECT c.clientid, c.clientname AS \"Клиент\", "
                    "COUNT(t.terminalid) AS \"Терминалов в аренде\" "
                    "FROM tblclients c "
                    "JOIN tblrentaldocs r ON c.clientid = r.clientid "
                    "JOIN tblrentaldetails rd ON r.rentaldocid = rd.rentaldocid "
                    "JOIN tblterminals t ON rd.terminalid = t.terminalid AND t.status = 1 "
                    "GROUP BY c.clientid, c.clientname "
                    "ORDER BY COUNT(t.terminalid) DESC",
                    m_db);
}

QVector<ClientRepository::RentalTerminal> ClientRepository::loadRentedTerminals(int clientId) const
{
    QVector<RentalTerminal> result;
    QSqlQuery query(m_db);
    query.prepare("SELECT m.modelname AS \"Модель\", "
                  "t.serialnumber AS \"Серийный номер\", "
                  "COALESCE(s.simnumber, '—') AS \"SIM-карта\", "
                  "CAST(r.docdate AS DATE) AS \"Дата передачи\" "
                  "FROM tblrentaldocs r "
                  "JOIN tblrentaldetails rd ON r.rentaldocid = rd.rentaldocid "
                  "JOIN tblterminals t ON rd.terminalid = t.terminalid AND t.status = 1 "
                  "LEFT JOIN tblmodels m ON t.modelid = m.modelid "
                  "LEFT JOIN tblsimcards s ON rd.simcardid = s.simcardid "
                  "WHERE r.clientid = :clientId "
                  "ORDER BY r.docdate DESC, t.serialnumber");
    query.bindValue(":clientId", clientId);
    if (!query.exec())
        return result;
    while (query.next()) {
        result.append({query.value(0).toString(), query.value(1).toString(), query.value(2).toString(),
                       query.value(3).toString()});
    }
    return result;
}

void ClientRepository::populateRentedTerminals(QSqlQueryModel* model, int clientId) const
{
    QSqlQuery query(m_db);
    query.prepare("SELECT m.modelname AS \"Модель\", "
                  "t.serialnumber AS \"Серийный номер\", "
                  "COALESCE(s.simnumber, '—') AS \"SIM-карта\", "
                  "CAST(r.docdate AS DATE) AS \"Дата передачи\" "
                  "FROM tblrentaldocs r "
                  "JOIN tblrentaldetails rd ON r.rentaldocid = rd.rentaldocid "
                  "JOIN tblterminals t ON rd.terminalid = t.terminalid AND t.status = 1 "
                  "LEFT JOIN tblmodels m ON t.modelid = m.modelid "
                  "LEFT JOIN tblsimcards s ON rd.simcardid = s.simcardid "
                  "WHERE r.clientid = :clientId "
                  "ORDER BY r.docdate DESC, t.serialnumber");
    query.bindValue(":clientId", clientId);
    if (query.exec())
        model->setQuery(std::move(query));
}