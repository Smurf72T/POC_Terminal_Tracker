#ifndef PAYMENTREPOSITORY_H
#define PAYMENTREPOSITORY_H

#include <QSqlDatabase>
#include <QString>
#include <QVector>

// Доступ к таблице tblpayments без SQL в UI-слое.
class PaymentRepository {
public:
    explicit PaymentRepository(const QSqlDatabase& db);

    struct MonthlyRevenue {
        QString month; // "YYYY-MM"
        double total = 0.0;
    };

    // Суммы оплат за последние months месяцев (включая текущий).
    QVector<MonthlyRevenue> revenueByMonth(int months = 6) const;

private:
    QSqlDatabase m_db;
};

#endif // PAYMENTREPOSITORY_H