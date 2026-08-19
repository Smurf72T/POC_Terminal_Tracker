#include "ui/base/transactionguard.h"
#include <QMessageBox>

TransactionGuard::TransactionGuard(QSqlDatabase& db) : m_db(db)
{
    if (!db.transaction()) {
        QMessageBox::critical(nullptr, "Ошибка", "Не удалось начать транзакцию");
    }
}

TransactionGuard::~TransactionGuard()
{
    if (m_committed)
        return;
    m_db.rollback();
    if (!m_rollbackMsg.isEmpty())
        QMessageBox::critical(nullptr, "Ошибка", m_rollbackMsg);
}

bool TransactionGuard::commit()
{
    if (!m_db.commit()) {
        m_db.rollback();
        m_committed = true;
        QMessageBox::critical(nullptr, "Ошибка", "Не удалось зафиксировать транзакцию");
        return false;
    }
    m_committed = true;
    return true;
}

void TransactionGuard::setRollbackMessage(const QString& msg)
{
    m_rollbackMsg = msg;
}