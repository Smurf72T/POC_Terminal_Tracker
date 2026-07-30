#include "databasemanager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
#include <QMap>
#include <QMessageBox>
#include <QSqlError>

static QMap<QString, QString> loadEnvFile(const QString &filePath)
{
    QMap<QString, QString> env;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return env;

    while (!file.atEnd()) {
        QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;
        int eq = line.indexOf('=');
        if (eq < 0)
            continue;
        env.insert(line.left(eq).trimmed(), line.mid(eq + 1).trimmed());
    }
    return env;
}

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

    // Ищем .env: рядом с executable, затем рядом с config.json, затем в корне проекта
    QFileInfo configInfo(configPath);
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList envCandidates = {
        appDir + "/.env",
        appDir + "/../.env",
        configInfo.absolutePath() + "/.env",
        configInfo.absolutePath() + "/../../.env"
    };

    QMap<QString, QString> env;
    for (const QString &candidate : envCandidates) {
        env = loadEnvFile(candidate);
        if (!env.isEmpty()) break;
    }

    QJsonObject dbConfig = m_config["database"].toObject();

    m_database = QSqlDatabase::addDatabase("QPSQL");
    m_database.setHostName(env.value("POC_DB_HOST", dbConfig["host"].toString()));
    int port = env.contains("POC_DB_PORT") ? env["POC_DB_PORT"].toInt() : dbConfig["port"].toInt();
    m_database.setPort(port);
    m_database.setDatabaseName(env.value("POC_DB_NAME", dbConfig["database"].toString()));
    m_database.setUserName(env.value("POC_DB_USER", dbConfig["username"].toString()));
    m_database.setPassword(env.value("POC_DB_PASSWORD", dbConfig["password"].toString()));

    // Пытаемся подключиться с SSL, если сервер не поддерживает — пробуем без SSL
    m_database.setConnectOptions("requiressl=1");

    if (!m_database.open()) {
        m_database.setConnectOptions("requiressl=0");
        if (!m_database.open()) {
            showError("Ошибка подключения к базе данных: " + m_database.lastError().text());
            return false;
        }
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

QSqlDatabase& DatabaseManager::getDatabase()
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

void DatabaseManager::notifyDataChanged()
{
    emit dataChanged();
}

void DatabaseManager::showError(const QString& message)
{
    QMessageBox::critical(nullptr, "Ошибка базы данных", message);
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

void DatabaseManager::logAction(const QString& action, const QString& tableName, int recordId,
                                const QString& username, const QString& oldValues,
                                const QString& newValues)
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
        qDebug() << "[AuditLog] Ошибка логирования:" << query.lastError().text();
    }
}

void DatabaseManager::setCurrentUser(const QString& username)
{
    m_currentUser = username;
}

QString DatabaseManager::getCurrentUser() const
{
    return m_currentUser;
}

void DatabaseManager::setCurrentUserRole(const QString& role)
{
    m_currentUserRole = role;
}

QString DatabaseManager::getCurrentUserRole() const
{
    return m_currentUserRole;
}

bool DatabaseManager::isCurrentUserAdmin() const
{
    return m_currentUserRole == "admin";
}