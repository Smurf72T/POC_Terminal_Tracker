#ifndef SIMCARDREPOSITORY_H
#define SIMCARDREPOSITORY_H

#include <QSqlDatabase>
#include <QSqlQueryModel>
#include <QString>
#include <QVector>

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

private:
    QSqlDatabase m_db;
};

#endif // SIMCARDREPOSITORY_H