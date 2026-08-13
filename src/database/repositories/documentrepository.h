#ifndef DOCUMENTREPOSITORY_H
#define DOCUMENTREPOSITORY_H

#include <QList>
#include <QSqlDatabase>
#include <QSqlQueryModel>
#include <QString>
#include <QVector>

#include "models/document.h"
#include "models/rentaldocument.h"

// Доступ к таблицам документов (поступление/аренда/возврат/изменение статуса)
// без SQL в UI-слое.
class DocumentRepository {
public:
    explicit DocumentRepository(const QSqlDatabase& db);

    // Типы документов (doctype) совпадают с кодами в recentDocuments:
    // 1 — поступление, 2 — аренда, 3 — возврат, 5 — изменение статуса.
    enum DocType : int { Receipt = 1, Rental = 2, Return = 3, StatusChange = 5 };

    struct RecentDocument {
        int docType = Receipt;
        int docId = 0;
        QString number;
        QString date;
        QString typeName;
    };

    // Последние документы всех типов, сортировка по дате (DESC), лимит.
    QVector<RecentDocument> recentDocuments(int limit = 15) const;
    void populateRecentDocuments(QSqlQueryModel* model, int limit = 15) const;

    // Шапка документа поступления/возврата.
    models::DocumentHeader loadHeader(DocType docType, int docId) const;

    // Аренда: шапка + строки.
    models::RentalDocument loadRentalDocument(int rentalDocId) const;
    QVector<models::RentalRow> loadRentalRows(int rentalDocId) const;
    // Документы аренды клиента (id, номер, дата) — для выпадающего списка.
    QVector<models::RentalDocument> loadRentalDocumentsByClient(int clientId) const;
    // Терминалы поступления (id, serial, modelid, imei1, imei2).
    QVector<models::ReceiptRow> loadReceiptRows(int receiptDocId) const;

    // Документ аренды, к которому относится возврат (-1, если не найден).
    int rentalDocIdForReturn(int returnDocId) const;
    // Терминалы, включённые в документ возврата.
    QList<int> returnedTerminalIds(int returnDocId) const;

private:
    QSqlDatabase m_db;
};

#endif // DOCUMENTREPOSITORY_H