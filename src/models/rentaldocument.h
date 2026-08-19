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
    int simCardId = 0;   // слот 1 (imei1)
    int simCard2Id = 0;  // слот 2 (imei2)
    QString terminalSerialNumber;
    QString simNumber;
    QString simNumber2;
    QString comment;
    int terminalStatus = 0;
    int simStatus = 0;
    int sim2Status = 0;
};

} // namespace models

#endif // RENTALDOCUMENT_H