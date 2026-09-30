#ifndef PAYMENTREPOSITORY_H
#define PAYMENTREPOSITORY_H

#include <QHash>
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

    // Сумма уже оплаченного по каждому документу аренды клиента:
    // ключ — rentaldocid, значение — сумма всех оплат, привязанных к документу.
    // excludePaymentId — платёж, который сейчас редактируется: его вклад не
    // учитывается, иначе в форме видна была бы сумма, которую редактируют.
    QHash<int, double> paidByRentalDocs(int clientId, int excludePaymentId = 0) const;

private:
    QSqlDatabase m_db;
};

#endif // PAYMENTREPOSITORY_H
