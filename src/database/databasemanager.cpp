#include "databasemanager.h"
#include <QCoreApplication>
#include <QDir>

DatabaseManager& DatabaseManager::instance()
{
    static DatabaseManager instance;
    return instance;
}

bool DatabaseManager::initialize(const QString& configPath)
{
    if (m_initialized) {
        return true;
    }

    if (!loadConfig(configPath)) {
        showError("Не удалось загрузить конфигурационный файл");
        return false;
    }

    QJsonObject dbConfig = m_config["database"].toObject();

    m_database = QSqlDatabase::addDatabase("QPSQL");
    m_database.setHostName(dbConfig["host"].toString());
    m_database.setPort(dbConfig["port"].toInt());
    m_database.setDatabaseName(dbConfig["database"].toString());
    m_database.setUserName(dbConfig["username"].toString());
    m_database.setPassword(dbConfig["password"].toString());

    if (!m_database.open()) {
        showError("Ошибка подключения к базе данных: " + m_database.lastError().text());
        return false;
    }

    m_initialized = true;
    return true;
}

bool DatabaseManager::loadConfig(const QString& configPath)
{
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray jsonData = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);

    if (doc.isNull()) {
        return false;
    }

    m_config = doc.object();
    return true;
}

bool DatabaseManager::isConnected() const
{
    return m_database.isOpen();
}

void DatabaseManager::close()
{
    if (m_database.isOpen()) {
        m_database.close();
    }
    m_initialized = false;
}

QSqlDatabase DatabaseManager::getDatabase() const
{
    return m_database;
}

QSqlQuery DatabaseManager::executeQuery(const QString& query, bool showErrorMessage)
{
    QSqlQuery sqlQuery(m_database);
    if (!sqlQuery.exec(query)) {
        if (showErrorMessage) {
            showError("Ошибка выполнения запроса: " + sqlQuery.lastError().text() +
                     "\nЗапрос: " + query);
        }
    }
    return sqlQuery;
}

bool DatabaseManager::executeTransaction(const std::function<bool(QSqlDatabase&)>& transactionFunc)
{
    if (!m_database.isOpen()) {
        showError("База данных не подключена");
        return false;
    }

    if (!m_database.transaction()) {
        showError("Не удалось начать транзакцию: " + m_database.lastError().text());
        return false;
    }

    bool success = transactionFunc(m_database);

    if (success) {
        if (!m_database.commit()) {
            showError("Не удалось зафиксировать транзакцию: " + m_database.lastError().text());
            m_database.rollback();
            return false;
        }
    } else {
        if (!m_database.rollback()) {
            showError("Не удалось откатить транзакцию: " + m_database.lastError().text());
        }
    }

    return success;
}

QString DatabaseManager::generateDocNumber(const QString& docType)
{
    QSqlQuery query(m_database);
    query.prepare("SELECT generate_doc_number(:doc_type)");
    query.bindValue(":doc_type", docType);

    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }

    showError("Не удалось сгенерировать номер документа");
    return "";
}

void DatabaseManager::showError(const QString& message)
{
    QMessageBox::critical(nullptr, "Ошибка базы данных", message);
}