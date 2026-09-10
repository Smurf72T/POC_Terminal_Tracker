#ifndef SIMINSTALLDOCUMENT_H
#define SIMINSTALLDOCUMENT_H

#include <QDate>
#include <QString>
#include <QVector>

namespace models {

struct SimInstallDocument {
    int id = 0;
    QString docNumber;
    QDate date;
    QString comments;
};

struct SimInstallRow {
    int detailId = 0;
    int terminalId = 0;
    QString terminalSerialNumber;
    int simCardId = 0;
    int simCard2Id = 0;
    QString simNumber;
    QString simNumber2;
};

} // namespace models

#endif // SIMINSTALLDOCUMENT_H
