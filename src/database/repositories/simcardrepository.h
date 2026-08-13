#ifndef SIMCARDREPOSITORY_H
#define SIMCARDREPOSITORY_H

#include <QList>
#include <QSqlDatabase>
#include <QSqlQueryModel>
#include <QString>
#include <QVector>

#include "models/simcard.h"

// Доступ к данным таблицы tblsimcards без SQL в UI-слое.
class SimCardRepository {
public:
    explicit SimCardRepository(const QSqlDatabase& db);

    struct FreeSimCard {
        QString simNumber;
        QString notes;
        QString status;
    };

    int countAll() const;
    int countFree() const;
    QVector<FreeSimCard> loadFreeSimCards() const;
    void populateFreeSimCards(QSqlQueryModel* model) const;

    // Полная модель SIM по id; invalid, если нет.
    models::SimCard loadById(int simCardId) const;
    // SIM по списку id (порядок вставки сохраняется, отсутствующие пропускаются).
    QVector<models::SimCard> loadByIds(const QList<int>& ids) const;
    // SIM для выбора в аренду: свободная или привязанная к свободному терминалу.
    QVector<models::SimCard> loadFreeForSelection() const;

private:
    QSqlDatabase m_db;
};

#endif // SIMCARDREPOSITORY_H