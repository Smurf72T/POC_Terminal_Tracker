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