#ifndef POSTACTIONLOGGER_H
#define POSTACTIONLOGGER_H

#include <QString>

// Логирование действия и оповещение об изменении данных после проведения
// документа. Инкапсулирует пару вызовов DatabaseManager::logAction + notifyDataChanged.
class PostActionLogger {
public:
    // Пишет запись в журнал действий (таблица audit).
    static void log(const QString& action, const QString& tableName, int recordId);
    // Оповещает остальные экземпляры приложения об изменении данных.
    static void notify();
};

#endif // POSTACTIONLOGGER_H
