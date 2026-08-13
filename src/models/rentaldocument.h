#ifndef RENTALDOCUMENT_H
#define RENTALDOCUMENT_H

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QVector>

// Value-модели арендного документа (tblrentaldocs + tblrentaldetails).
// Наполняются через DocumentRepository.
namespace models {

struct RentalDocument {
    int id = 0;
    QString docNumber;
    QDate date;
    int clientId = 0;
    QString comments;
};

struct RentalRow {
    int rentalDetailId = 0;
    int terminalId = 0;
    int simCardId = 0;
    QString terminalSerialNumber;
    QString simNumber;
    QString comment;
    int terminalStatus = 0;
    int simStatus = 0;
};

} // namespace models

#endif // RENTALDOCUMENT_H