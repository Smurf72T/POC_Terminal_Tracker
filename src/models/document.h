#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <QDate>
#include <QString>

// Общая шапка документа (поступление / возврат / изменение статуса).
// Наполняется через DocumentRepository. Для документов без клиента
// clientId остаётся 0.
namespace models {

struct DocumentHeader {
    int id = 0;
    QString docNumber;
    QDate date;
    int clientId = 0;
    QString comments;
};

// Строка документа поступления (tblreceiptdetails + tblterminals).
struct ReceiptRow {
    int terminalId = 0;
    QString serialNumber;
    int modelId = 0;
    QString modelName;
    QString imei1;
    QString imei2;
};

} // namespace models

#endif // DOCUMENT_H