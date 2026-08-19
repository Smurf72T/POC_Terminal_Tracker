#include "databasemanager.h"
#include "utils/logging.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>

namespace {

constexpr int kMaxTxnRetries = 3;

bool isTransientError(const QSqlError& err)
{
    const QString sqlState = err.nativeErrorCode().trimmed().toUpper();
    if (sqlState.isEmpty())
        return false;
    if (sqlState.startsWith("08")) // класс ошибок соединения
        return true;
    return sqlState == "40P01"     // deadlock_detected
           || sqlState == "40001"  // serialization_failure
           || sqlState == "57P01"; // admin_shutdown
}

} // namespace

QSqlQuery DatabaseManager::executeQuery(const QString& query, bool showErrorMessage)
{
    if (!m_circuitBreaker.isAllowed()) {
        // Запрос не выполняется: circuit breaker открыт после серии сбоев БД.
        // Не выполняем заведомо некорректный SQL ради заполнения lastError —
        // возвращаем невыполненный запрос и сообщаем о проблеме.
        const QString message =
            QStringLiteral("База данных временно недоступна: слишком много неудачных запросов подряд. "
                           "Повторите действие через несколько секунд.");
        qCWarning(logDB) << message;
        if (showErrorMessage)
            showError(message);
        return QSqlQuery(m_database);
    }

    QSqlQuery sqlQuery(m_database);
    if (sqlQuery.exec(query)) {
        m_circuitBreaker.onSuccess();
    } else {
        m_circuitBreaker.onFailure();
        if (showErrorMessage) {
            showError("Ошибка выполнения запроса: " + sqlQuery.lastError().text() + "\nЗапрос: " + query);
        }
    }
    return sqlQuery;
}

bool DatabaseManager::executeTransaction(const std::function<bool(QSqlDatabase&)>& transactionFunc)
{
    if (!m_circuitBreaker.isAllowed()) {
        showError("База данных временно недоступна: слишком много неудачных запросов подряд. "
                  "Повторите действие через несколько секунд.");
        return false;
    }

    for (int attempt = 1; attempt <= kMaxTxnRetries; ++attempt) {
        if (!m_database.isOpen()) {
            if (!m_database.open()) {
                showError("База данных не подключена");
                m_circuitBreaker.onFailure();
                return false;
            }
            m_circuitBreaker.onSuccess();
        }

        if (!m_database.transaction()) {
            showError("Не удалось начать транзакцию: " + m_database.lastError().text());
            m_circuitBreaker.onFailure();
            return false;
        }

        bool success = transactionFunc(m_database);

        if (!success) {
            if (!m_database.rollback()) {
                showError("Не удалось откатить транзакцию: " + m_database.lastError().text());
            }
            return false;
        }

        if (m_database.commit()) {
            m_circuitBreaker.onSuccess();
            return true;
        }

        QSqlError commitError = m_database.lastError();
        m_database.rollback();

        if (!isTransientError(commitError) || attempt == kMaxTxnRetries) {
            showError("Не удалось зафиксировать транзакцию: " + commitError.text());
            m_circuitBreaker.onFailure();
            return false;
        }

        qCWarning(logDB) << "Транзакция отменена transient-ошибкой (попытка" << attempt << "из" << kMaxTxnRetries
                         << "):" << commitError.text();
        QThread::msleep(100 * attempt);
    }

    return false;
}

QString DatabaseManager::generateDocNumber(const QString& docType)
{
    QSqlQuery query(m_database);
    query.prepare("SELECT generate_doc_number(:type)");
    query.bindValue(":type", docType);

    if (!query.exec() || !query.next()) {
        showError("Не удалось сгенерировать номер документа: " + query.lastError().text());
        return QString();
    }

    return query.value(0).toString();
}

void DatabaseManager::logAction(const QString& action, const QString& tableName, int recordId, const QString& username,
                                const QString& oldValues, const QString& newValues)
{
    QSqlQuery query(m_database);
    query.prepare("SELECT log_audit_action(:action, :table, :recid, :uname, :oldv, :newv)");
    query.bindValue(":action", action);
    query.bindValue(":table", tableName);
    query.bindValue(":recid", recordId);
    query.bindValue(":uname", username.isEmpty() ? m_currentUser : username);
    query.bindValue(":oldv", oldValues);
    query.bindValue(":newv", newValues);

    if (!query.exec()) {
        qCWarning(logAudit) << "Ошибка логирования:" << query.lastError().text();
    }
}
