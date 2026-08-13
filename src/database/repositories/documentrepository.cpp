#include "database/repositories/documentrepository.h"

#include <QSqlQuery>

#include <utility>

DocumentRepository::DocumentRepository(const QSqlDatabase& db) : m_db(db) {}

QVector<DocumentRepository::RecentDocument> DocumentRepository::recentDocuments(int limit) const
{
    QVector<RecentDocument> result;
    QSqlQuery query(m_db);
    query.prepare("SELECT 1 AS doctype, receiptdocid AS docid, docnumber AS \"Номер\", docdate AS \"Дата\", "
                  "'Поступление' AS \"Тип\" FROM tblreceiptdocs "
                  "UNION ALL "
                  "SELECT 2, rentaldocid, docnumber, docdate, 'Аренда' FROM tblrentaldocs "
                  "UNION ALL "
                  "SELECT 3, returndocid, docnumber, docdate, 'Возврат' FROM tblreturndocs "
                  "UNION ALL "
                  "SELECT 5, statuschangedocid, docnumber, docdate, 'Изменение статуса' FROM tblstatuschangedocs "
                  "ORDER BY \"Дата\" DESC "
                  "LIMIT :limit");
    query.bindValue(":limit", limit);
    if (!query.exec())
        return result;
    while (query.next()) {
        result.append({query.value(0).toInt(), query.value(1).toInt(), query.value(2).toString(),
                       query.value(3).toString(), query.value(4).toString()});
    }
    return result;
}

void DocumentRepository::populateRecentDocuments(QSqlQueryModel* model, int limit) const
{
    QSqlQuery query(m_db);
    query.prepare("SELECT 1 AS doctype, receiptdocid AS docid, docnumber AS \"Номер\", docdate AS \"Дата\", "
                  "'Поступление' AS \"Тип\" FROM tblreceiptdocs "
                  "UNION ALL "
                  "SELECT 2, rentaldocid, docnumber, docdate, 'Аренда' FROM tblrentaldocs "
                  "UNION ALL "
                  "SELECT 3, returndocid, docnumber, docdate, 'Возврат' FROM tblreturndocs "
                  "UNION ALL "
                  "SELECT 5, statuschangedocid, docnumber, docdate, 'Изменение статуса' FROM tblstatuschangedocs "
                  "ORDER BY \"Дата\" DESC "
                  "LIMIT :limit");
    query.bindValue(":limit", limit);
    if (query.exec())
        model->setQuery(std::move(query));
}

models::DocumentHeader DocumentRepository::loadHeader(DocType docType, int docId) const
{
    QSqlQuery query(m_db);
    switch (docType) {
        case Receipt:
            query.prepare("SELECT docnumber, docdate, comments FROM tblreceiptdocs WHERE receiptdocid = :id");
            break;
        case Return:
            query.prepare("SELECT docnumber, docdate, clientid, comments FROM tblreturndocs WHERE returndocid = :id");
            break;
        case StatusChange:
            query.prepare("SELECT docnumber, docdate, 0 AS clientid, comment AS comments "
                          "FROM tblstatuschangedocs WHERE statuschangedocid = :id");
            break;
        default:
            return {};
    }
    query.bindValue(":id", docId);
    if (!query.exec() || !query.next())
        return {};
    models::DocumentHeader h;
    h.id = docId;
    h.docNumber = query.value(0).toString();
    h.date = query.value(1).toDateTime().date();
    h.clientId = query.value(2).toInt();
    h.comments = query.value(3).toString();
    return h;
}

models::RentalDocument DocumentRepository::loadRentalDocument(int rentalDocId) const
{
    QSqlQuery query(m_db);
    query.prepare("SELECT docnumber, docdate, clientid, comments FROM tblrentaldocs WHERE rentaldocid = :id");
    query.bindValue(":id", rentalDocId);
    if (!query.exec() || !query.next())
        return {};
    models::RentalDocument d;
    d.id = rentalDocId;
    d.docNumber = query.value(0).toString();
    d.date = query.value(1).toDateTime().date();
    d.clientId = query.value(2).toInt();
    d.comments = query.value(3).toString();
    return d;
}

QVector<models::RentalRow> DocumentRepository::loadRentalRows(int rentalDocId) const
{
    QVector<models::RentalRow> result;
    QSqlQuery query(m_db);
    query.prepare("SELECT rd.rentaldetailid, rd.terminalid, rd.simcardid, "
                  "t.serialnumber, COALESCE(s.simnumber, ''), rd.comment, "
                  "t.status AS terminal_status, s.status AS sim_status "
                  "FROM tblrentaldetails rd "
                  "JOIN tblterminals t ON rd.terminalid = t.terminalid "
                  "LEFT JOIN tblsimcards s ON rd.simcardid = s.simcardid "
                  "WHERE rd.rentaldocid = :docid "
                  "ORDER BY t.serialnumber");
    query.bindValue(":docid", rentalDocId);
    if (!query.exec())
        return result;
    while (query.next()) {
        models::RentalRow row;
        row.rentalDetailId = query.value(0).toInt();
        row.terminalId = query.value(1).toInt();
        row.simCardId = query.value(2).toInt();
        row.terminalSerialNumber = query.value(3).toString();
        row.simNumber = query.value(4).toString();
        row.comment = query.value(5).toString();
        row.terminalStatus = query.value(6).toInt();
        row.simStatus = query.value(7).toInt();
        result.append(row);
    }
    return result;
}

QVector<models::RentalDocument> DocumentRepository::loadRentalDocumentsByClient(int clientId) const
{
    QVector<models::RentalDocument> result;
    QSqlQuery query(m_db);
    query.prepare("SELECT rentaldocid, docnumber, docdate FROM tblrentaldocs "
                  "WHERE clientid = :cid ORDER BY docdate DESC");
    query.bindValue(":cid", clientId);
    if (!query.exec())
        return result;
    while (query.next()) {
        models::RentalDocument d;
        d.id = query.value(0).toInt();
        d.docNumber = query.value(1).toString();
        d.date = query.value(2).toDateTime().date();
        result.append(d);
    }
    return result;
}

QVector<models::ReceiptRow> DocumentRepository::loadReceiptRows(int receiptDocId) const
{
    QVector<models::ReceiptRow> result;
    QSqlQuery query(m_db);
    query.prepare("SELECT t.terminalid, t.serialnumber, t.modelid, COALESCE(m.modelname, ''), "
                  "t.imei1, t.imei2 "
                  "FROM tblreceiptdetails rd "
                  "JOIN tblterminals t ON rd.terminalid = t.terminalid "
                  "LEFT JOIN tblmodels m ON t.modelid = m.modelid "
                  "WHERE rd.receiptdocid = :id");
    query.bindValue(":id", receiptDocId);
    if (!query.exec())
        return result;
    while (query.next()) {
        models::ReceiptRow row;
        row.terminalId = query.value(0).toInt();
        row.serialNumber = query.value(1).toString();
        row.modelId = query.value(2).toInt();
        row.modelName = query.value(3).toString();
        row.imei1 = query.value(4).toString();
        row.imei2 = query.value(5).toString();
        result.append(row);
    }
    return result;
}

int DocumentRepository::rentalDocIdForReturn(int returnDocId) const
{
    QSqlQuery query(m_db);
    query.prepare("SELECT DISTINCT rd.rentaldocid "
                  "FROM tblrentaldetails rd "
                  "JOIN tblreturndetails rtd ON rd.terminalid = rtd.terminalid "
                  "WHERE rtd.returndocid = :id "
                  "LIMIT 1");
    query.bindValue(":id", returnDocId);
    if (!query.exec() || !query.next())
        return -1;
    return query.value(0).toInt();
}

QList<int> DocumentRepository::returnedTerminalIds(int returnDocId) const
{
    QList<int> result;
    QSqlQuery query(m_db);
    query.prepare("SELECT terminalid FROM tblreturndetails WHERE returndocid = :id");
    query.bindValue(":id", returnDocId);
    if (!query.exec())
        return result;
    while (query.next())
        result.append(query.value(0).toInt());
    return result;
}