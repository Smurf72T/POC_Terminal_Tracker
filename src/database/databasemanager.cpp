#include "databasemanager.h"
#include "utils/logging.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
#include <QMap>
#include <QMessageBox>
#include <QSqlDriver>
#include <QSqlError>
#include <QTextStream>

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

    // SSL mode: require | prefer | disable
    QString sslMode = env.value("POC_DB_SSLMODE", dbConfig["sslmode"].toString("prefer")).toLower().trimmed();

    if (sslMode == "require") {
        m_database.setConnectOptions("requiressl=1");
        if (!m_database.open()) {
            showError("Ошибка подключения к БД: сервер не поддерживает SSL (sslmode=require).\n"
                      + m_database.lastError().text());
            return false;
        }
    } else if (sslMode == "disable") {
        m_database.setConnectOptions("requiressl=0");
        if (!m_database.open()) {
            showError("Ошибка подключения к базе данных:\n" + m_database.lastError().text());
            return false;
        }
    } else {
        // prefer — пробуем SSL, при неудаче предупреждаем и подключаемся без SSL
        m_database.setConnectOptions("requiressl=1");
        if (!m_database.open()) {
            qCWarning(logDB) << "SSL не поддерживается сервером, подключаемся без SSL";
            m_database.setConnectOptions("requiressl=0");
            if (!m_database.open()) {
                showError("Ошибка подключения к базе данных:\n" + m_database.lastError().text());
                return false;
            }
        }
    }

    if (!runMigrations()) {
        showError("Не удалось применить миграции базы данных: " + m_database.lastError().text());
        close();
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
    QWidget *parent = QApplication::activeWindow();
    QMessageBox::critical(parent, "Ошибка базы данных", message);
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
        qCWarning(logAudit) << "Ошибка логирования:" << query.lastError().text();
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

bool DatabaseManager::ensureMigrationsTable()
{
    QSqlQuery q(m_database);
    return q.exec(
        "CREATE TABLE IF NOT EXISTS schema_migrations ("
        "  version VARCHAR(255) PRIMARY KEY,"
        "  applied_at TIMESTAMP DEFAULT NOW()"
        ")"
    );
}

QStringList DatabaseManager::pendingMigrations()
{
    QStringList pending;
    if (!ensureMigrationsTable()) {
        qCCritical(logMigration) << "Не удалось создать schema_migrations:" << m_database.lastError().text();
        return pending;
    }

    QSet<QString> applied;
    QSqlQuery q(m_database);
    if (q.exec("SELECT version FROM schema_migrations ORDER BY version")) {
        while (q.next()) {
            applied.insert(q.value(0).toString());
        }
    }

    QString migrationsDir;
    QStringList candidates = {
        QCoreApplication::applicationDirPath() + "/sql/migrations/",
        QCoreApplication::applicationDirPath() + "/../sql/migrations/",
        QCoreApplication::applicationDirPath() + "/../../sql/migrations/"
    };
    for (const QString &c : candidates) {
        QDir d(c);
        if (d.exists()) { migrationsDir = d.absolutePath(); break; }
    }
    if (migrationsDir.isEmpty()) {
        qCInfo(logMigration) << "Директория миграций не найдена";
        return pending;
    }

    QDirIterator it(migrationsDir, QStringList() << "*.sql", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QString fileName = it.fileName();
        if (!applied.contains(it.fileName())) {
            pending.append(it.filePath());
        }
    }
    pending.sort();
    return pending;
}

bool DatabaseManager::runMigrations(const QString &migrationsDir)
{
    Q_UNUSED(migrationsDir);
    QStringList pending = pendingMigrations();
    if (pending.isEmpty()) {
        return true;
    }

    qCInfo(logMigration) << "Найдено ожидающих миграций:" << pending.size();

    for (const QString &filePath : pending) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCWarning(logMigration) << "Не удалось открыть:" << filePath;
            return false;
        }
        QTextStream in(&file);
        QString sql = in.readAll();
        file.close();

        QFileInfo fi(filePath);
        QString version = fi.fileName();

        if (!m_database.transaction()) {
            qCWarning(logMigration) << "Не удалось начать транзакцию:" << m_database.lastError().text();
            return false;
        }

        QSqlQuery q(m_database);
        if (!q.exec(sql)) {
            qCWarning(logMigration) << "Ошибка в" << version << ":" << q.lastError().text();
            m_database.rollback();
            return false;
        }

        if (m_database.driver()->hasFeature(QSqlDriver::Transactions)) {
            if (!m_database.commit()) {
                qCWarning(logMigration) << "Не удалось закоммитить:" << m_database.lastError().text();
                m_database.rollback();
                return false;
            }
        }

        QSqlQuery rec(m_database);
        rec.prepare("INSERT INTO schema_migrations (version) VALUES (:v)");
        rec.bindValue(":v", version);
        if (!rec.exec()) {
            qCWarning(logMigration) << "Не удалось записать версию:" << rec.lastError().text();
            return false;
        }

        qCInfo(logMigration) << "Применена:" << version;
    }

    return true;
}