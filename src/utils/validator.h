#ifndef VALIDATOR_H
#define VALIDATOR_H

#include <QObject>
#include <QString>
#include <QRegularExpression>
#include <QSqlQuery>
#include <QSqlError>

class Validator : public QObject {
    Q_OBJECT

public:
    explicit Validator(QObject* parent = nullptr);

    static bool validateIMEI(const QString& imei);
    static bool validateINN(const QString& inn);
    static bool validateSerialNotEmpty(const QString& serial);
    static bool checkUniqueSerial(const QString& serial, int excludeTerminalId = -1);
    static bool checkDuplicateIMEI(const QString& imei, int excludeTerminalId = -1);
    static bool checkLuhn(const QString& number);
    static bool validateINNChecksum(const QString& inn);

    static QRegularExpression createIMEIValidator();
    static QRegularExpression createINNValidator();
    static QRegularExpression createSerialValidator();

private:
    static QRegularExpression& imeiRegex();
    static QRegularExpression& innRegex();
};

#endif // VALIDATOR_H
