#ifndef CLIENTREPOSITORY_H
#define CLIENTREPOSITORY_H

#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QString>
#include <QVector>

// Доступ к данным таблицы tblclients без SQL в UI-слое.
class ClientRepository {
public:
    explicit ClientRepository(const QSqlDatabase& db);

    struct RentalStatistic {
        int clientId = 0;
        QString clientName;
        int activeTerminals = 0;
    };

    struct RentalTerminal {
        QString modelName;
        QString serialNumber;
        QString simNumber;
        QString transferDate;
    };

    int countAll() const;
    // Терминалы в аренде по клиентам (col: clientid, clientname, count).
    QVector<RentalStatistic> loadRentalStatistics() const;
    void populateRentalStatistics(QSqlQueryModel* model) const;

    // Терминалы клиента в аренде (для отчёта «Клиент — Терминалы в аренде»).
    QVector<RentalTerminal> loadRentedTerminals(int clientId) const;
    void populateRentedTerminals(QSqlQueryModel* model, int clientId) const;

private:
    QSqlDatabase m_db;
};

#endif // CLIENTREPOSITORY_H