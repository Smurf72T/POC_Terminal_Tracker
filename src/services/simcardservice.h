#ifndef SIMCARDSERVICE_H
#define SIMCARDSERVICE_H

#include <QString>

class QSqlDatabase;

// Бизнес-логика работы с SIM-картами в документах аренды:
// привязка/освобождение SIM и создание новой SIM по номеру из ячейки.
class SimCardService {
public:
    // Если в ячейке задан только номер SIM (id = 0) — находит свободную
    // существующую карту или создаёт новую. Возвращает id SIM или -1 при ошибке
    // (текст ошибки — в *error).
    static int resolveOrCreate(QSqlDatabase& db, int cellSimId, const QString& number, QString* error);
    // Блокирует SIM (статус 0 -> 1) с защитой FOR UPDATE NOWAIT.
    // false при занятой карте или ошибке (текст — в *error).
    static bool lock(QSqlDatabase& db, int simId, const QString& context, QString* error);
    // Освобождает SIM (статус 1 -> 0). false при ошибке (текст — в *error).
    static bool free(QSqlDatabase& db, int simId, const QString& context, QString* error);
};

#endif // SIMCARDSERVICE_H