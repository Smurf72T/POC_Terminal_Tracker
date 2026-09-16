#ifndef CASESERVICE_H
#define CASESERVICE_H

#include <QString>

class QSqlDatabase;

// Бизнес-логика работы с чехлами: установка/снятие/списание и авто-выдача
// свободного чехла при проведении аренды с пометкой «с чехлом».
class CaseService {
public:
    // Устанавливает свободный чехол (status 0 -> 1) на терминал (FOR UPDATE NOWAIT).
    // false при занятом/списанном чехле или ошибке (текст — в *error).
    static bool lock(QSqlDatabase& db, int caseId, int terminalId, const QString& context, QString* error);
    // Освобождает чехол (status 1 -> 0) и снимает привязку терминала.
    static bool free(QSqlDatabase& db, int caseId, const QString& context, QString* error);
    // Списывает чехол (status -> 2), при установке — отвязывает от терминала.
    static bool writeoff(QSqlDatabase& db, int caseId, const QString& context, QString* error);
    // Выдаёт любой свободный чехол на терминал. Возвращает caseId или 0
    // (текст ошибки — в *error).
    static int assignAnyFree(QSqlDatabase& db, int terminalId, const QString& context, QString* error);
};

#endif // CASESERVICE_H