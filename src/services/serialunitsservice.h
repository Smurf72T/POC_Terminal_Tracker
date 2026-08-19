#ifndef SERIALUNITSSERVICE_H
#define SERIALUNITSSERVICE_H

#include <QString>
#include <QStringList>

// Логика работы с комплектами «серийник + IMEI 1 + IMEI 2» в форме поступления.
// Чистые функции без Qt-виджетов — легко тестировать.
class SerialUnitsService {
public:
    // Краткое описание комплектов для ячейки таблицы: «введено/ожидалось · превью».
    static QString summary(const QStringList& values, int expected);
    // Выравнивает список IMEI под число серийников: пустой список превращается
    // в список пустых строк нужной длины.
    static QStringList alignedImei(const QStringList& serials, const QStringList& imei);
    // true, если IMEI — ровно 15 цифр.
    static bool isValidImei(const QString& imei);
    // Проверка серийного номера комплекта: непустой после обрезки.
    static bool isValidSerial(const QString& serialNumber);
};

#endif // SERIALUNITSSERVICE_H