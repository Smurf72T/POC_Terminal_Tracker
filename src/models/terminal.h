#ifndef TERMINAL_H
#define TERMINAL_H

#include <QDate>
#include <QString>

// Value-модель POC-терминала (tblterminals). Не содержит логики доступа к БД;
// наполняется репозиториями (TerminalRepository).
namespace models {

struct Terminal {
    int id = 0;
    QString serialNumber;
    int modelId = 0;
    QString modelName;
    QString imei1;
    QString imei2;
    int status = 0;
    bool deactivated = false;
    int currentSimCardId = 0;
    QDate purchaseDate;
    QString notes;
    bool wasRepaired = false;
};

} // namespace models

#endif // TERMINAL_H