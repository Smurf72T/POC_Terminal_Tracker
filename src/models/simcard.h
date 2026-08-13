#ifndef SIMCARD_H
#define SIMCARD_H

#include <QDate>
#include <QString>

// Value-модель SIM-карты (tblsimcards). Наполняется через SimCardRepository.
namespace models {

struct SimCard {
    int id = 0;
    QString number;
    int status = 0;
    QString notes;
    QDate createdAt;
};

} // namespace models

#endif // SIMCARD_H