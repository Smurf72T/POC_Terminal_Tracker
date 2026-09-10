#include "siminstallrepository.h"
#include <QSqlQuery>
#include <QSqlError>

SimInstallRepository::SimInstallRepository(const QSqlDatabase& db) : m_db(db) {}

models::SimInstallDocument SimInstallRepository::loadHeader(int docId) const
{
    models::SimInstallDocument d;
    QSqlQuery query(m_db);
    query.prepare("SELECT docnumber, docdate, comments FROM tblsiminstalldocs WHERE siminstalldocid = :id");
    query.bindValue(":id", docId);
    if (!query.exec() || !query.next())
        return d;
    d.id = docId;
    d.docNumber = query.value(0).toString();
    d.date = query.value(1).toDateTime().date();
    d.comments = query.value(2).toString();
    return d;
}

QVector<models::SimInstallRow> SimInstallRepository::loadDetails(int docId) const
{
    QVector<models::SimInstallRow> result;
    QSqlQuery query(m_db);
    query.prepare("SELECT d.siminstalldetailid, d.terminalid, t.serialnumber, "
                  "d.simcardid, d.simcardid2, "
                  "COALESCE(s.simnumber, ''), COALESCE(s2.simnumber, '') "
                  "FROM tblsiminstalldetails d "
                  "JOIN tblterminals t ON d.terminalid = t.terminalid "
                  "LEFT JOIN tblsimcards s ON d.simcardid = s.simcardid "
                  "LEFT JOIN tblsimcards s2 ON d.simcardid2 = s2.simcardid "
                  "WHERE d.siminstalldocid = :docid "
                  "ORDER BY t.serialnumber");
    query.bindValue(":docid", docId);
    if (!query.exec())
        return result;
    while (query.next()) {
        models::SimInstallRow row;
        row.detailId = query.value(0).toInt();
        row.terminalId = query.value(1).toInt();
        row.terminalSerialNumber = query.value(2).toString();
        row.simCardId = query.value(3).toInt();
        row.simCard2Id = query.value(4).toInt();
        row.simNumber = query.value(5).toString();
        row.simNumber2 = query.value(6).toString();
        result.append(row);
    }
    return result;
}

int SimInstallRepository::createHeader(const models::SimInstallDocument& doc) const
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO tblsiminstalldocs (docnumber, docdate, comments) "
                  "VALUES (:num, :date, :comm) RETURNING siminstalldocid");
    query.bindValue(":num", doc.docNumber);
    query.bindValue(":date", QDateTime(doc.date, QTime::currentTime()));
    query.bindValue(":comm", doc.comments);
    if (!query.exec() || !query.next())
        return -1;
    return query.value(0).toInt();
}

bool SimInstallRepository::insertDetail(int docId, int terminalId, int simCardId, int simCard2Id) const
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO tblsiminstalldetails (siminstalldocid, terminalid, simcardid, simcardid2) "
                  "VALUES (:did, :tid, :sid, :sid2)");
    query.bindValue(":did", docId);
    query.bindValue(":tid", terminalId);
    query.bindValue(":sid", simCardId > 0 ? QVariant(simCardId) : QVariant());
    query.bindValue(":sid2", simCard2Id > 0 ? QVariant(simCard2Id) : QVariant());
    return query.exec();
}

bool SimInstallRepository::deleteDetails(int docId) const
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM tblsiminstalldetails WHERE siminstalldocid = :id");
    query.bindValue(":id", docId);
    return query.exec();
}

bool SimInstallRepository::deleteHeader(int docId) const
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM tblsiminstalldocs WHERE siminstalldocid = :id");
    query.bindValue(":id", docId);
    return query.exec();
}
