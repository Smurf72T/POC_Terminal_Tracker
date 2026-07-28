#ifndef VALIDATOR_H
#define VALIDATOR_H

#include <QObject>
#include <QString>
#include <QRegularExpression>
#include <QSqlQuery>
#include <QSqlError>

class Validator : public QObject
{
    Q_OBJECT

public:
    explicit Validator(QObject *parent = nullptr);

    // Валидация IMEI (строго 15 цифр)
    static bool validateIMEI(const QString& imei);

    // Валидация ИНН (10 или 12 цифр)
    static bool validateINN(const QString& inn);

    // Проверка серийного номера на непустоту
    static bool validateSerialNotEmpty(const QString& serial);

    // Проверка уникальности серийного номера
    static bool checkUniqueSerial(const QString& serial, int excludeTerminalId = -1);

    // Создание QRegularExpression для IMEI
    static QRegularExpression createIMEIValidator();

    // Создание QRegularExpression для ИНН
    static QRegularExpression createINNValidator();

    // Создание QRegularExpression для серийного номера (не пустой, только допустимые символы)
    static QRegularExpression createSerialValidator();

private:
    static QRegularExpression s_imeiRegex;
    static QRegularExpression s_innRegex;
};

#endif // VALIDATOR_H
