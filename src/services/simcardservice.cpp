#include "services/simcardservice.h"
#include "database/databasemanager.h"
#include <QSqlError>
#include <QSqlQuery>

int SimCardService::resolveOrCreate(QSqlDatabase& db, int cellSimId, const QString& number, QString* error)
{
    if (cellSimId > 0 || number.isEmpty())
        return cellSimId;

    if (number.length() > 19) {
        *error = QString("Номер SIM-карты «%1» слишком длинный (макс. 19 символов).").arg(number);
        return -1;
    }

    QSqlQuery findSim(db);
    findSim.prepare("SELECT simcardid, status FROM tblsimcards WHERE simnumber = :n");
    findSim.bindValue(":n", number);
    if (findSim.exec() && findSim.next()) {
        const int simId = findSim.value(0).toInt();
        if (findSim.value(1).toInt() != 0) {
            *error = QString("SIM-карта %1 уже занята!").arg(number);
            return -1;
        }
        return simId;
    }

    QSqlQuery insertSim(db);
    insertSim.prepare("INSERT INTO tblsimcards (simnumber, status) VALUES (:n, 0) RETURNING simcardid");
    insertSim.bindValue(":n", number);
    if (!insertSim.exec() || !insertSim.next()) {
        *error = QString("Не удалось создать SIM-карту %1: %2").arg(number).arg(insertSim.lastError().text());
        return -1;
    }
    const int simId = insertSim.value(0).toInt();
    DatabaseManager::instance().logAction("INSERT", "tblsimcards", simId, QString(), QString(),
                                          QString("simnumber=%1").arg(number));
    return simId;
}

bool SimCardService::lock(QSqlDatabase& db, int simId, const QString& context, QString* error)
{
    QSqlQuery simLock(db);
    simLock.prepare("SELECT status FROM tblsimcards WHERE simcardid = :id AND status = 0 FOR UPDATE NOWAIT");
    simLock.bindValue(":id", simId);
    if (!simLock.exec() || !simLock.next()) {
        *error = QString("%1 уже занята другим терминалом!").arg(context);
        return false;
    }

    QSqlQuery simQuery(db);
    simQuery.prepare("UPDATE tblsimcards SET status = 1 WHERE simcardid = :id");
    simQuery.bindValue(":id", simId);
    if (!simQuery.exec()) {
        *error = QString("Не удалось обновить %1: %2").arg(context).arg(simQuery.lastError().text());
        return false;
    }
    return true;
}

bool SimCardService::free(QSqlDatabase& db, int simId, const QString& context, QString* error)
{
    QSqlQuery freeOld(db);
    freeOld.prepare("UPDATE tblsimcards SET status = 0 WHERE simcardid = :id");
    freeOld.bindValue(":id", simId);
    if (!freeOld.exec()) {
        *error = QString("Не удалось освободить SIM-карту %1 (%2): %3")
                     .arg(simId)
                     .arg(context)
                     .arg(freeOld.lastError().text());
        return false;
    }
    return true;
}