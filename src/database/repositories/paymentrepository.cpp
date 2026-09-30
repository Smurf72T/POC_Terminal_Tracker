#include "database/repositories/paymentrepository.h"

#include "utils/logging.h"

#include <QDate>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>

#include <algorithm>

PaymentRepository::PaymentRepository(const QSqlDatabase& db) : m_db(db) {}

QVector<PaymentRepository::MonthlyRevenue> PaymentRepository::revenueByMonth(int months) const
{
    QVector<MonthlyRevenue> result;
    if (months <= 0)
        return result;

    const QDate now = QDate::currentDate();
    const int currentIndex = now.year() * 12 + now.month();
    const int floorIndex = currentIndex - (months - 1);

    QSqlQuery query(m_db);
    query.prepare("SELECT periodyear, periodmonth, COALESCE(SUM(amount), 0) AS total "
                  "FROM tblpayments "
                  "WHERE periodyear * 12 + periodmonth >= :floor "
                  "GROUP BY periodyear, periodmonth "
                  "ORDER BY periodyear, periodmonth");
    query.bindValue(":floor", floorIndex);
    if (!query.exec())
        return result;

    QSet<int> seenIndexes;
    while (query.next()) {
        const int year = query.value(0).toInt();
        const int month = query.value(1).toInt();
        const double total = query.value(2).toDouble();
        result.append({QString("%1-%2").arg(year, 4, 10, QLatin1Char('0')).arg(month, 2, 10, QLatin1Char('0')), total});
        seenIndexes.insert(year * 12 + month);
    }

    // Дополняем месяцы, для которых нет записей, нулевыми значениями —
    // чтобы график не «проваливался».
    for (int i = floorIndex; i <= currentIndex; ++i) {
        if (!seenIndexes.contains(i)) {
            result.append(
                {QString("%1-%2").arg(i / 12, 4, 10, QLatin1Char('0')).arg(i % 12, 2, 10, QLatin1Char('0')), 0.0});
        }
    }

    std::sort(result.begin(), result.end(),
              [](const MonthlyRevenue& a, const MonthlyRevenue& b) { return a.month < b.month; });

    return result;
}

QHash<int, double> PaymentRepository::paidByRentalDocs(int clientId, int excludePaymentId) const
{
    QHash<int, double> result;
    if (clientId == 0)
        return result;

    QSqlQuery query(m_db);
    query.prepare("SELECT pl.rentaldocid, COALESCE(SUM(p.amount), 0) "
                  "FROM tblpayment_rental_links pl "
                  "JOIN tblpayments p ON p.paymentid = pl.paymentid "
                  "WHERE p.clientid = :cid AND (:exclude = 0 OR pl.paymentid <> :exclude) "
                  "GROUP BY pl.rentaldocid");
    query.bindValue(":cid", clientId);
    query.bindValue(":exclude", excludePaymentId);

    if (!query.exec()) {
        qCWarning(logDB) << "Не удалось получить суммы оплат по документам аренды:" << query.lastError().text();
        return result;
    }

    while (query.next())
        result.insert(query.value(0).toInt(), query.value(1).toDouble());

    return result;
}
