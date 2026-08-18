#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <QDate>
#include <QString>
#include <QVector>

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

// Комплект серийника строки поступления (tblreceiptserials): linenum — номер
// по порядку внутри строки документа.
struct ReceiptSerial {
    int linenum = 0;
    QString serialNumber;
    QString imei1;
    QString imei2;
};

// Строка «исходника» документа поступления (tblreceiptitems):
// модель + кол-во + списки серийников/IMEI, как их вводил пользователь.
struct ReceiptItem {
    int itemId = 0;
    int modelId = 0;
    QString modelName;
    int qty = 0;
    QVector<ReceiptSerial> serials;
};

} // namespace models

#endif // DOCUMENT_H