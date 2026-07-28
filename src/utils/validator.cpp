#include "validator.h"
#include "database/databasemanager.h"

QRegularExpression Validator::s_imeiRegex;
QRegularExpression Validator::s_innRegex;

Validator::Validator(QObject *parent) : QObject(parent)
{
    // Инициализация регулярных выражений
    // IMEI: ровно 15 цифр
    s_imeiRegex = QRegularExpression("^\\d{15}$");
    // ИНН: 10 или 12 цифр
    s_innRegex = QRegularExpression("^\\d{10}$|^\\d{12}$");
}

bool Validator::validateIMEI(const QString& imei)
{
    return s_imeiRegex.match(imei).hasMatch();
}

bool Validator::validateINN(const QString& inn)
{
    return s_innRegex.match(inn).hasMatch();
}

bool Validator::validateSerialNotEmpty(const QString& serial)
{
    return !serial.trimmed().isEmpty() && serial.trimmed().length() >= 3;
}

bool Validator::checkUniqueSerial(const QString& serial, int excludeTerminalId)
{
    QSqlQuery query(DatabaseManager::instance().getDatabase());

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
        return false;
    }

    if (query.next()) {
        return query.value(0).toInt() == 0;
    }

    return false;
}

QRegularExpression* Validator::createIMEIValidator()
{
    return new QRegularExpression(s_imeiRegex);
}

QRegularExpression* Validator::createINNValidator()
{
    return new QRegularExpression(s_innRegex);
}

QRegularExpression* Validator::createSerialValidator()
{
    // Серийный номер: минимум 3 символа, допустимы буквы, цифры, дефис, точка
    QRegularExpression* regex = new QRegularExpression("^[A-Za-z0-9\\-\\.]{3,50}$");
    return regex;
}
