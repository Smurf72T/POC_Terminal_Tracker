#ifndef SIMINSTALLREPOSITORY_H
#define SIMINSTALLREPOSITORY_H

#include <QSqlDatabase>
#include <QVector>

#include "models/siminstalldocument.h"

class SimInstallRepository {
public:
    explicit SimInstallRepository(const QSqlDatabase& db);

    models::SimInstallDocument loadHeader(int docId) const;
    QVector<models::SimInstallRow> loadDetails(int docId) const;

    // Записать шапку документа установки SIM. Возвращает id или -1 при ошибке.
    int createHeader(const models::SimInstallDocument& doc) const;
    // Записать строку документа.
    bool insertDetail(int docId, int terminalId, int simCardId, int simCard2Id) const;
    // Удалить все строки документа (при редактировании).
    bool deleteDetails(int docId) const;
    // Удалить шапку документа.
    bool deleteHeader(int docId) const;

private:
    QSqlDatabase m_db;
};

#endif // SIMINSTALLREPOSITORY_H
