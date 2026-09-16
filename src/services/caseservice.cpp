#include "services/caseservice.h"

#include "database/databasemanager.h"
#include <QSqlError>
#include <QSqlQuery>

bool CaseService::lock(QSqlDatabase& db, int caseId, int terminalId, const QString& context, QString* error)
{
    QSqlQuery caseLock(db);
    caseLock.prepare("SELECT status FROM tblcases WHERE caseid = :id AND status = 0 FOR UPDATE NOWAIT");
    caseLock.bindValue(":id", caseId);
    if (!caseLock.exec() || !caseLock.next()) {
        *error = QString("%1 уже установлен или списан!").arg(context);
        return false;
    }

    QSqlQuery updateCase(db);
    updateCase.prepare("UPDATE tblcases SET status = 1, terminalid = :tid WHERE caseid = :id");
    updateCase.bindValue(":tid", terminalId);
    updateCase.bindValue(":id", caseId);
    if (!updateCase.exec()) {
        *error = QString("Не удалось обновить %1: %2").arg(context).arg(updateCase.lastError().text());
        return false;
    }

    QSqlQuery updateTerminal(db);
    updateTerminal.prepare("UPDATE tblterminals SET currentcaseid = :cid WHERE terminalid = :tid");
    updateTerminal.bindValue(":cid", caseId);
    updateTerminal.bindValue(":tid", terminalId);
    if (!updateTerminal.exec()) {
        *error = QString("Не удалось привязать чехол к терминалу: %1").arg(updateTerminal.lastError().text());
        return false;
    }
    return true;
}

bool CaseService::free(QSqlDatabase& db, int caseId, const QString& context, QString* error)
{
    QSqlQuery clearTerminal(db);
    clearTerminal.prepare("UPDATE tblterminals SET currentcaseid = NULL WHERE currentcaseid = :id");
    clearTerminal.bindValue(":id", caseId);
    if (!clearTerminal.exec()) {
        *error = QString("Не удалось снять чехол с терминала: %1").arg(clearTerminal.lastError().text());
        return false;
    }

    QSqlQuery freeCase(db);
    freeCase.prepare("UPDATE tblcases SET status = 0, terminalid = NULL WHERE caseid = :id AND status = 1");
    freeCase.bindValue(":id", caseId);
    if (!freeCase.exec()) {
        *error = QString("Не удалось освободить %1: %2").arg(context).arg(freeCase.lastError().text());
        return false;
    }
    return true;
}

bool CaseService::writeoff(QSqlDatabase& db, int caseId, const QString& context, QString* error)
{
    QSqlQuery clearTerminal(db);
    clearTerminal.prepare("UPDATE tblterminals SET currentcaseid = NULL WHERE currentcaseid = :id");
    clearTerminal.bindValue(":id", caseId);
    if (!clearTerminal.exec()) {
        *error = QString("Не удалось снять списываемый чехол с терминала: %1").arg(clearTerminal.lastError().text());
        return false;
    }

    QSqlQuery writeoffCase(db);
    writeoffCase.prepare("UPDATE tblcases SET status = 2, terminalid = NULL WHERE caseid = :id AND status <> 2");
    writeoffCase.bindValue(":id", caseId);
    if (!writeoffCase.exec()) {
        *error = QString("Не удалось списать %1: %2").arg(context).arg(writeoffCase.lastError().text());
        return false;
    }
    return true;
}

int CaseService::assignAnyFree(QSqlDatabase& db, int terminalId, const QString& /*context*/, QString* error)
{
    QSqlQuery pick(db);
    pick.prepare("SELECT caseid FROM tblcases WHERE status = 0 "
                 "ORDER BY caseid FOR UPDATE SKIP LOCKED LIMIT 1");
    if (!pick.exec() || !pick.next()) {
        *error = QString("Нет свободных чехлов для терминала %1.").arg(terminalId);
        return 0;
    }
    const int caseId = pick.value(0).toInt();

    QString lockError;
    if (!lock(db, caseId, terminalId, QString("Чехол №%1").arg(caseId), &lockError)) {
        *error = lockError;
        return 0;
    }
    return caseId;
}