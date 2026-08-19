#include "databasemanager.h"
#include "utils/logging.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QMap>
#include <QSqlError>
#include <QSqlQuery>

static QMap<QString, QString> loadEnvFile(const QString& filePath)
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

// На Linux предупреждает, если .env доступен на запись группе или остальным
// (права 0644/0664): файл содержит секреты (пароль БД/кодовая фраза бэкапа).
static void warnOnInsecureEnvPermissions(const QString& filePath)
{
#ifdef Q_OS_LINUX
    QFileInfo info(filePath);
    if (!info.exists())
        return;
    const QFileInfo::Permissions perms = info.permissions();
    const bool groupWritable = perms & QFileInfo::WriteGroup;
    const bool otherWritable = perms & QFileInfo::WriteOther;
    if (groupWritable || otherWritable) {
        qCWarning(logApp) << "Файл .env (" << filePath << ") содержит секреты, но доступен"
                          << "на запись другим пользователям. Выполните: chmod 600" << filePath;
    }
#else
    Q_UNUSED(filePath);
#endif
}

bool DatabaseManager::openConnection()
{
    // Ищем .env: рядом с executable, затем рядом с config.json, затем в корне проекта
    QFileInfo configInfo(m_configPath);
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList envCandidates = {appDir + "/.env", appDir + "/../.env", configInfo.absolutePath() + "/.env",
                                 configInfo.absolutePath() + "/../.env"};

    QMap<QString, QString> env;
    QString loadedEnvPath;
    for (const QString& candidate : envCandidates) {
        QMap<QString, QString> candidateEnv = loadEnvFile(candidate);
        if (!candidateEnv.isEmpty()) {
            env = candidateEnv;
            loadedEnvPath = candidate;
            break;
        }
    }
    if (!loadedEnvPath.isEmpty()) {
        qCInfo(logApp) << "DatabaseManager: данные подключаются из .env:" << loadedEnvPath;
        warnOnInsecureEnvPermissions(loadedEnvPath);
    } else {
        qCInfo(logApp) << "DatabaseManager: .env не найден — используются значения из config.json";
    }

    QJsonObject dbConfig = m_config["database"].toObject();

    m_database = QSqlDatabase::addDatabase("QPSQL");
    m_database.setHostName(env.value("POC_DB_HOST", dbConfig["host"].toString()));
    int port = env.contains("POC_DB_PORT") ? env["POC_DB_PORT"].toInt() : dbConfig["port"].toInt();
    m_database.setPort(port);
    m_database.setDatabaseName(env.value("POC_DB_NAME", dbConfig["database"].toString()));
    m_database.setUserName(env.value("POC_DB_USER", dbConfig["username"].toString()));
    m_database.setPassword(env.value("POC_DB_PASSWORD", dbConfig["password"].toString()));

    // SSL mode: disable | prefer | require | verify-ca | verify-full
    // По умолчанию require: без SSL подключение не выполняется (защита от MITM).
    QString sslMode = env.value("POC_DB_SSLMODE", dbConfig["sslmode"].toString("require")).toLower().trimmed();
    QString sslRootCert = env.value("POC_DB_SSLROOTCERT", dbConfig["sslrootcert"].toString()).trimmed();

    if (sslMode == "verify-full" || sslMode == "verify-ca") {
        // Проверка сертификата сервера (снимает риск MITM при sslmode=prefer)
        QString opts = QString("sslmode=%1").arg(sslMode);
        if (!sslRootCert.isEmpty()) {
            opts += ";sslrootcert=" + sslRootCert;
        }
        m_database.setConnectOptions(opts);
        if (!m_database.open()) {
            showError(QString("Ошибка подключения к БД: серверный сертификат не прошёл проверку (sslmode=%1).\n"
                              "Укажите корректный путь к корневому сертификату (sslrootcert).\n%2")
                          .arg(sslMode, m_database.lastError().text()));
            return false;
        }
    } else if (sslMode == "require") {
        m_database.setConnectOptions("sslmode=require");
        if (!m_database.open()) {
            showError("Ошибка подключения к БД: сервер не поддерживает SSL (sslmode=require).\n" +
                      m_database.lastError().text());
            return false;
        }
    } else if (sslMode == "disable") {
        m_database.setConnectOptions("sslmode=disable");
        if (!m_database.open()) {
            showError("Ошибка подключения к базе данных:\n" + m_database.lastError().text());
            return false;
        }
    } else {
        // prefer — пробуем SSL, при неудаче предупреждаем и подключаемся без SSL.
        // ВАЖНО: проверка сертификата при prefer не выполняется — для защиты
        // соединения используйте sslmode=verify-full с sslrootcert.
        m_database.setConnectOptions("sslmode=require");
        if (!m_database.open()) {
            qCWarning(logDB) << "SSL не поддерживается сервером, подключаемся без SSL";
            m_database.setConnectOptions("sslmode=disable");
            if (!m_database.open()) {
                showError("Ошибка подключения к базе данных:\n" + m_database.lastError().text());
                return false;
            }
        } else {
            qCWarning(logDB) << "Подключено по SSL без проверки сертификата (sslmode=prefer). "
                             << "Для проверки сертификата используйте sslmode=verify-full";
        }
    }

    return true;
}

bool DatabaseManager::checkConnection(const QString& configPath, QString* error)
{
    if (m_initialized) {
        return true;
    }

    if (!loadConfig(configPath)) {
        if (error)
            *error = "Не удалось загрузить конфигурационный файл";
        return false;
    }

    if (!openConnection()) {
        if (error)
            *error = m_database.lastError().text();
        return false;
    }

    QSqlQuery q(m_database);
    if (!q.exec("SELECT 1") || !q.next()) {
        if (error)
            *error = q.lastError().text();
        close();
        return false;
    }
    return true;
}
