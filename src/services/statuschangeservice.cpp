#include "services/statuschangeservice.h"
#include "utils/terminal_status.h"

int StatusChangeService::targetStatus(const QString& actionType)
{
    if (actionType == "repair")
        return 2;
    if (actionType == "repair_return")
        return 0;
    if (actionType == "writeoff")
        return 3;
    if (actionType == "lost")
        return 4;
    return 0;
}

bool StatusChangeService::expectStatus(const QString& actionType, int currentStatus)
{
    if (actionType == "repair")
        return currentStatus == 0;
    if (actionType == "repair_return")
        return currentStatus == 2;
    if (actionType == "writeoff")
        return currentStatus == 0 || currentStatus == 2;
    if (actionType == "lost")
        return currentStatus == 1 || currentStatus == 2;
    return false;
}

QString StatusChangeService::actionTitle(const QString& actionType)
{
    if (actionType == "repair")
        return "В ремонт";
    if (actionType == "repair_return")
        return "Возврат из ремонта";
    if (actionType == "writeoff")
        return "Списан";
    if (actionType == "lost")
        return "Утерян";
    return "Изменение статуса";
}

QString StatusChangeService::statusText(int status)
{
    return TerminalStatus::name(status);
}