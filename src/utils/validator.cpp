#include "validator.h"
#include "database/idatabasemanager.h"

QRegularExpression& Validator::imeiRegex()
{
    static QRegularExpression re("^\\d{15}$");
    return re;
}

QRegularExpression& Validator::innRegex()
{
    static QRegularExpression re("^\\d{10}$|^\\d{12}$");
    return re;
}

Validator::Validator(QObject *parent) : QObject(parent)
{
}

bool Validator::validateIMEI(const QString& imei)
{
    QString cleaned = imei;
    cleaned.remove(QRegularExpression("[^\\d]"));
    return imeiRegex().match(cleaned).hasMatch();
}

bool Validator::validateINN(const QString& inn)
{
    if (!innRegex().match(inn).hasMatch()) return false;
    return validateINNChecksum(inn);
}

bool Validator::validateSerialNotEmpty(const QString& serial)
{
    return !serial.trimmed().isEmpty() && serial.trimmed().length() >= 3;
}

bool Validator::checkUniqueSerial(const QString& serial, int excludeTerminalId)
{
    QSqlQuery query(databaseManager().getDatabase());

    if (excludeTerminalId >= 0) {
        query.prepare("SELECT COUNT(*) FROM tblterminals "
                      "WHERE serialnumber = :sn AND terminalid != :id");
        query.bindValue(":sn", serial.trimmed());
        query.bindValue(":id", excludeTerminalId);
    } else {
        query.prepare("SELECT COUNT(*) FROM tblterminals "
                      "WHERE serialnumber = :sn");
        query.bindValue(":sn", serial.trimmed());
    }

    if (!query.exec()) {
        return true;
    }

    if (query.next()) {
        return query.value(0).toInt() == 0;
    }

    return true;
}

bool Validator::checkLuhn(const QString& number)
{
    if (number.isEmpty()) return false;
    int sum = 0;
    bool alternate = false;
    for (int i = number.length() - 1; i >= 0; --i) {
        int digit = number[i].digitValue();
        if (digit < 0) return false;
        if (alternate) {
            digit *= 2;
            if (digit > 9) digit -= 9;
        }
        sum += digit;
        alternate = !alternate;
    }
    return (sum % 10) == 0;
}

bool Validator::validateINNChecksum(const QString& inn)
{
    if (inn.length() == 10) {
        int weights10[] = {2, 4, 10, 3, 5, 9, 4, 6, 8};
        int sum = 0;
        for (int i = 0; i < 9; ++i) {
            sum += inn[i].digitValue() * weights10[i];
        }
        int checkDigit = (sum % 11) % 10;
        return checkDigit == inn[9].digitValue();
    }
    if (inn.length() == 12) {
        int weights12[] = {7, 2, 4, 10, 3, 5, 9, 4, 6, 8};
        int sum1 = 0;
        for (int i = 0; i < 10; ++i) {
            sum1 += inn[i].digitValue() * weights12[i];
        }
        int check1 = (sum1 % 11) % 10;
        if (check1 != inn[10].digitValue()) return false;

        int weights12b[] = {3, 7, 2, 4, 10, 3, 5, 9, 4, 6, 8};
        int sum2 = 0;
        for (int i = 0; i < 11; ++i) {
            sum2 += inn[i].digitValue() * weights12b[i];
        }
        int check2 = (sum2 % 11) % 10;
        return check2 == inn[11].digitValue();
    }
    return false;
}

bool Validator::checkDuplicateIMEI(const QString& imei, int excludeTerminalId)
{
    if (imei.isEmpty() || imei == "000000000000000") return false;

    QSqlQuery query(databaseManager().getDatabase());
    if (excludeTerminalId >= 0) {
        query.prepare("SELECT COUNT(*) FROM tblterminals "
                      "WHERE (imei1 = :imei OR imei2 = :imei) AND terminalid != :id");
        query.bindValue(":imei", imei);
        query.bindValue(":id", excludeTerminalId);
    } else {
        query.prepare("SELECT COUNT(*) FROM tblterminals "
                      "WHERE imei1 = :imei OR imei2 = :imei");
        query.bindValue(":imei", imei);
    }

    if (!query.exec()) return true;
    if (query.next()) return query.value(0).toInt() > 0;
    return true;
}

QRegularExpression Validator::createIMEIValidator()
{
    return QRegularExpression(imeiRegex());
}

QRegularExpression Validator::createINNValidator()
{
    return QRegularExpression(innRegex());
}

QRegularExpression Validator::createSerialValidator()
{
    return QRegularExpression("^[A-Za-z0-9\\-\\.]{3,50}$");
}
