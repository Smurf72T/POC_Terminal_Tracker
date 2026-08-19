#ifndef TRANSACTIONGUARD_H
#define TRANSACTIONGUARD_H

#include <QSqlDatabase>
#include <QString>

// RAII-обёртка SQL-транзакции: автоматический rollback при раннем return
// или исключении, явный commit через commit().
class TransactionGuard {
public:
    explicit TransactionGuard(QSqlDatabase& db);
    ~TransactionGuard();

    // Фиксирует транзакцию. При ошибке — откат и сообщение, false.
    bool commit();
    // Сообщение, показываемое при откате (если задано).
    void setRollbackMessage(const QString& msg);

    TransactionGuard(const TransactionGuard&) = delete;
    TransactionGuard& operator=(const TransactionGuard&) = delete;

private:
    QSqlDatabase& m_db;
    bool m_committed = false;
    QString m_rollbackMsg;
};

#endif // TRANSACTIONGUARD_H
