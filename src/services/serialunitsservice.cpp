#include "services/serialunitsservice.h"

QString SerialUnitsService::summary(const QStringList& values, int expected)
{
    if (values.isEmpty())
        return "—";
    QString preview = values.join("; ");
    if (preview.size() > 34)
        preview = preview.left(34) + "…";
    return QString("%1/%2 · %3").arg(values.size()).arg(expected).arg(preview);
}

QStringList SerialUnitsService::alignedImei(const QStringList& serials, const QStringList& imei)
{
    if (!imei.isEmpty())
        return imei;
    return QStringList(serials.size(), QString());
}

bool SerialUnitsService::isValidImei(const QString& imei)
{
    return imei.size() == 15;
}

bool SerialUnitsService::isValidSerial(const QString& serialNumber)
{
    return !serialNumber.trimmed().isEmpty();
}