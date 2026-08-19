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

models::Client ClientRepository::loadById(int clientId) const
{
    QSqlQuery query(m_db);
    query.prepare("SELECT clientid, clientname, inn, address, contactphone, contactemail "
                  "FROM tblclients WHERE clientid = :id");
    query.bindValue(":id", clientId);
    if (!query.exec() || !query.next())
        return {};
    models::Client c;
    c.id = query.value(0).toInt();
    c.name = query.value(1).toString();
    c.inn = query.value(2).toString();
    c.address = query.value(3).toString();
    c.contactPhone = query.value(4).toString();
    c.contactEmail = query.value(5).toString();
    return c;
}

QVector<models::Client> ClientRepository::loadAll() const
{
    QVector<models::Client> result;
    QSqlQuery query(m_db);
    if (query.exec("SELECT clientid, clientname, inn, address, contactphone, contactemail "
                   "FROM tblclients ORDER BY clientname")) {
        while (query.next()) {
            models::Client c;
            c.id = query.value(0).toInt();
            c.name = query.value(1).toString();
            c.inn = query.value(2).toString();
            c.address = query.value(3).toString();
            c.contactPhone = query.value(4).toString();
            c.contactEmail = query.value(5).toString();
            result.append(c);
        }
    }
    return result;
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
                  "TRIM(COALESCE(s.simnumber, '') || CASE WHEN s2.simnumber IS NOT NULL "
                  "      THEN '; ' || s2.simnumber ELSE '' END) AS \"SIM-карта\", "
                  "CAST(r.docdate AS DATE) AS \"Дата передачи\" "
                  "FROM tblrentaldocs r "
                  "JOIN tblrentaldetails rd ON r.rentaldocid = rd.rentaldocid "
                  "JOIN tblterminals t ON rd.terminalid = t.terminalid AND t.status = 1 "
                  "LEFT JOIN tblmodels m ON t.modelid = m.modelid "
                  "LEFT JOIN tblsimcards s ON rd.simcardid = s.simcardid "
                  "LEFT JOIN tblsimcards s2 ON rd.simcardid2 = s2.simcardid "
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
                  "TRIM(COALESCE(s.simnumber, '') || CASE WHEN s2.simnumber IS NOT NULL "
                  "      THEN '; ' || s2.simnumber ELSE '' END) AS \"SIM-карта\", "
                  "CAST(r.docdate AS DATE) AS \"Дата передачи\" "
                  "FROM tblrentaldocs r "
                  "JOIN tblrentaldetails rd ON r.rentaldocid = rd.rentaldocid "
                  "JOIN tblterminals t ON rd.terminalid = t.terminalid AND t.status = 1 "
                  "LEFT JOIN tblmodels m ON t.modelid = m.modelid "
                  "LEFT JOIN tblsimcards s ON rd.simcardid = s.simcardid "
                  "LEFT JOIN tblsimcards s2 ON rd.simcardid2 = s2.simcardid "
                  "WHERE r.clientid = :clientId "
                  "ORDER BY r.docdate DESC, t.serialnumber");
    query.bindValue(":clientId", clientId);
    if (query.exec())
        model->setQuery(std::move(query));
}