#ifndef STATUSCHANGESERVICE_H
#define STATUSCHANGESERVICE_H

#include <QString>

// Бизнес-логика документа «Изменение статуса»: правила переходов между
// статусами терминала для каждого типа действия.
class StatusChangeService {
public:
    // Целевой статус для типа действия ("repair", "repair_return", "writeoff", "lost").
    static int targetStatus(const QString& actionType);
    // Допустим ли переход из текущего статуса для данного типа действия.
    static bool expectStatus(const QString& actionType, int currentStatus);
    // Человекочитаемое название типа действия.
    static QString actionTitle(const QString& actionType);
    // Название статуса терминала (обёртка над TerminalStatus::name).
    static QString statusText(int status);
};

#endif // STATUSCHANGESERVICE_H