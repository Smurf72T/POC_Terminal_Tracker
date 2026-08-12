#include "databasemanager.h"
#include "utils/logging.h"
#include "utils/password_utils.h"
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
#include <QStringConverter>
#include <QTextStream>
#include <QThread>
#include <QRandomGenerator>
#include <cstdio>

namespace {

constexpr int kMaxTxnRetries = 3;

bool isTransientError(const QSqlError &err)
{
    const QString sqlState = err.nativeErrorCode().trimmed().toUpper();
    if (sqlState.isEmpty())
        return false;
    if (sqlState.startsWith("08"))  // класс ошибок соединения
        return true;
    return sqlState == "40P01"   // deadlock_detected
        || sqlState == "40001"   // serialization_failure
        || sqlState == "57P01";  // admin_shutdown
}

} // namespace

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

// На Linux предупреждает, если .env доступен на запись группе или остальным
// (права 0644/0664): файл содержит секреты (пароль БД/кодовая фраза бэкапа).
void warnOnInsecureEnvPermissions(const QString &filePath)
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

DatabaseManager& DatabaseManager::instance()
{
    static DatabaseManager instance;
    return instance;
}

bool DatabaseManager::s_suppressDialogs = false;

void DatabaseManager::setSuppressDialogs(bool suppress)
{
    s_suppressDialogs = suppress;
}

bool DatabaseManager::suppressDialogs()
{
    return s_suppressDialogs;
}

IDatabaseManager &databaseManager()
{
    return DatabaseManager::instance();
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

    if (!openConnection()) {
        return false;
    }

    if (!runMigrations()) {
        showError("Не удалось применить миграции базы данных: " + m_database.lastError().text());
        close();
        return false;
    }

    seedAdminAccount();

    listenForDataChanges();

    m_initialized = true;
    return true;
}

bool DatabaseManager::openConnection()
{
    // Ищем .env: рядом с executable, затем рядом с config.json, затем в корне проекта
    QFileInfo configInfo(m_configPath);
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList envCandidates = {
        appDir + "/.env",
        appDir + "/../.env",
        configInfo.absolutePath() + "/.env",
        configInfo.absolutePath() + "/../../.env"
    };

    QMap<QString, QString> env;
    QString loadedEnvPath;
    for (const QString &candidate : envCandidates) {
        QMap<QString, QString> candidateEnv = loadEnvFile(candidate);
        if (!candidateEnv.isEmpty()) {
            env = candidateEnv;
            loadedEnvPath = candidate;
            break;
        }
    }
    if (!loadedEnvPath.isEmpty())
        warnOnInsecureEnvPermissions(loadedEnvPath);

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
            showError("Ошибка подключения к БД: сервер не поддерживает SSL (sslmode=require).\n"
                      + m_database.lastError().text());
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

bool DatabaseManager::checkConnection(const QString &configPath, QString *error)
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

void DatabaseManager::listenForDataChanges()
{
    if (m_listening || !m_database.isOpen())
        return;

    // Подписка на канал NOTIFY 'poc_data_changed' (триггеры из 007-й миграции).
    // Доставленные уведомления эмитятся в dataChanged() — дашборды всех
    // запущенных экземпляров обновляются при изменениях в любом из них.
    QSqlDriver *driver = m_database.driver();
    if (!driver->hasFeature(QSqlDriver::DriverFeature::EventNotifications))
        return;

    if (!driver->subscribeToNotification(QStringLiteral("poc_data_changed"))) {
        qCWarning(logDB) << "Не удалось подписаться на уведомления БД:"
                         << m_database.lastError().text();
        return;
    }

    connect(driver, &QSqlDriver::notification, this,
            [this](const QString & /*channel*/,
                   QSqlDriver::NotificationSource /*source*/,
                   const QVariant & /*payload*/) { emit dataChanged(); });

    m_listening = true;
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
    m_configPath = configPath;
    return true;
}

bool DatabaseManager::isConnected() const
{
    return m_database.isOpen();
}

void DatabaseManager::close()
{
    if (m_listening) {
        m_database.driver()->unsubscribeFromNotification(QStringLiteral("poc_data_changed"));
        m_listening = false;
    }
    if (m_database.isOpen()) {
        m_database.close();
    }
    m_initialized = false;
}

const QSqlDatabase &DatabaseManager::getDatabase() const
{
    return m_database;
}

QJsonObject DatabaseManager::configObject() const
{
    return m_config;
}

QSqlQuery DatabaseManager::executeQuery(const QString& query, bool showErrorMessage)
{
    if (!m_circuitBreaker.isAllowed()) {
        // Запрос не выполняется: circuit breaker открыт после серии сбоев БД.
        // Не выполняем заведомо некорректный SQL ради заполнения lastError —
        // возвращаем невыполненный запрос и сообщаем о проблеме.
        const QString message = QStringLiteral(
            "База данных временно недоступна: слишком много неудачных запросов подряд. "
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
            showError("Ошибка выполнения запроса: " + sqlQuery.lastError().text() +
                     "\nЗапрос: " + query);
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

        qCWarning(logDB) << "Транзакция отменена transient-ошибкой (попытка"
                         << attempt << "из" << kMaxTxnRetries << "):" << commitError.text();
        QThread::msleep(100 * attempt);
    }

    return false;
}

void DatabaseManager::notifyDataChanged()
{
    emit dataChanged();
}

void DatabaseManager::showError(const QString& message)
{
    if (s_suppressDialogs) {
        qCCritical(logDB) << message;
        return;
    }
    // В рабочем потоке (бэкап, миграции) модальный диалог заблокировал бы и поток,
    // и цикл событий UI — логируем вместо показа.
    if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
        qCCritical(logDB) << message;
        return;
    }
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

void DatabaseManager::setAuditUsername(const QString& username)
{
    // set_config(..., false) — параметр живёт до конца сессии.
    // Триггеры аудита (tblterminals/tblclients) читают current_setting('app.username').
    QSqlQuery query(m_database);
    query.prepare("SELECT set_config('app.username', :uname, false)");
    query.bindValue(":uname", username);
    if (!query.exec()) {
        qCWarning(logAudit) << "Не удалось установить app.username:" << query.lastError().text();
    }
}

void DatabaseManager::setSessionRole(const QString& role)
{
    // set_config(..., false) — параметр живёт до конца сессии.
    // Триггеры авторизации (tbl_users, tbl_audit_log) читают current_setting('app.role').
    QSqlQuery query(m_database);
    query.prepare("SELECT set_config('app.role', :role, false)");
    query.bindValue(":role", role);
    if (!query.exec()) {
        qCWarning(logAudit) << "Не удалось установить app.role:" << query.lastError().text();
    }
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

QStringList DatabaseManager::pendingMigrationsReadOnly() const
{
    QStringList pending;

    // НЕ создаём schema_migrations (read-only диагностика): если таблицы нет —
    // БД свежая, все миграции считаются ожидающими.
    QSet<QString> applied;
    QSqlQuery probe(m_database);
    if (probe.exec("SELECT to_regclass('public.schema_migrations')") && probe.next()) {
        if (!probe.value(0).isNull()) {
            QSqlQuery q(m_database);
            if (q.exec("SELECT version FROM schema_migrations ORDER BY version")) {
                while (q.next())
                    applied.insert(q.value(0).toString());
            }
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
        if (!applied.contains(fileName))
            pending.append(it.filePath());
    }
    pending.sort();
    return pending;
}

bool DatabaseManager::runMigrations(const QString &migrationsDir)
{
    Q_UNUSED(migrationsDir);

    // Advisory lock защищает от гонки при одновременном старте нескольких
    // экземпляров приложения: пока один применяет миграции, остальные ждут,
    // после чего их список pending будет пуст (версии уже записаны).
    static const qint64 kMigrationLockKey = 0x504F434D494752; // "POCMIGR"
    QSqlQuery lockQuery(m_database);
    lockQuery.prepare("SELECT pg_advisory_lock(:key)");
    lockQuery.bindValue(":key", kMigrationLockKey);
    if (!lockQuery.exec()) {
        qCWarning(logMigration) << "Не удалось взять advisory lock на миграции:"
                                << lockQuery.lastError().text();
        return false;
    }

    bool ok = applyPendingMigrations();

    QSqlQuery unlockQuery(m_database);
    unlockQuery.prepare("SELECT pg_advisory_unlock(:key)");
    unlockQuery.bindValue(":key", kMigrationLockKey);
    if (!unlockQuery.exec()) {
        qCWarning(logMigration) << "Не удалось снять advisory lock:"
                                << unlockQuery.lastError().text();
    }

    return ok;
}

bool DatabaseManager::applyPendingMigrations()
{
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
        in.setEncoding(QStringConverter::Utf8);
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

        // Версию записываем в той же транзакции: при сбое записи миграция
        // откатится целиком, и при следующем запуске она применится заново.
        QSqlQuery rec(m_database);
        rec.prepare("INSERT INTO schema_migrations (version) VALUES (:v)");
        rec.bindValue(":v", version);
        if (!rec.exec()) {
            qCWarning(logMigration) << "Не удалось записать версию:" << rec.lastError().text();
            m_database.rollback();
            return false;
        }

        if (!m_database.commit()) {
            qCWarning(logMigration) << "Не удалось закоммитить:" << m_database.lastError().text();
            m_database.rollback();
            return false;
        }

        qCInfo(logMigration) << "Применена:" << version;
    }

    return true;
}

void DatabaseManager::seedAdminAccount()
{
    // Дефолтная учётная запись admin не содержится в миграциях (пароль не должен
    // храниться в репозитории). Здесь после применения миграций создаём её со
    // случайным паролем и выводим пароль один раз. must_change_password=TRUE —
    // пароль придётся сменить при первом входе.
    QSqlQuery exists(m_database);
    exists.prepare("SELECT password_hash FROM tbl_users WHERE username = 'admin'");
    if (exists.exec() && exists.next()) {
        QString hash = exists.value(0).toString().trimmed();
        if (hash.isEmpty()) {
            const QString password = generateRandomPassword();
            QSqlQuery upd(m_database);
            upd.prepare("UPDATE tbl_users SET password_hash = :h, must_change_password = TRUE "
                        "WHERE username = 'admin'");
            upd.bindValue(":h", hashPassword(password));
            if (upd.exec()) {
                qCInfo(logDB) << "Учётной записи admin задан новый случайный пароль";
                printOneTimeAdminPassword(password);
            } else {
                qCCritical(logDB) << "Не удалось установить пароль admin:"
                                  << upd.lastError().text();
            }
        }
        return;
    }

    const QString password = generateRandomPassword();
    QSqlQuery ins(m_database);
    ins.prepare("INSERT INTO tbl_users (username, display_name, password_hash, role, is_active, must_change_password) "
                "VALUES ('admin', 'Администратор', :h, 'admin', TRUE, TRUE)");
    ins.bindValue(":h", hashPassword(password));
    if (ins.exec()) {
        qCInfo(logDB) << "Создана учётная запись admin со случайным паролем";
        printOneTimeAdminPassword(password);
    } else {
        qCCritical(logDB) << "Не удалось создать учётную запись admin:"
                          << ins.lastError().text();
    }
}

QString DatabaseManager::generateRandomPassword()
{
    const QString chars =
        "abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    QString password;
    password.reserve(16);
    QRandomGenerator *rng = QRandomGenerator::global();
    for (int i = 0; i < 16; ++i)
        password += chars.at(rng->bounded(chars.size()));
    return password;
}

void DatabaseManager::printOneTimeAdminPassword(const QString &password)
{
    // Печатаем пароль один раз в stdout (--check-db / первый запуск).
    std::printf("\n============================================================\n"
                "Создана учётная запись admin со СЛУЧАЙНЫМ паролем.\n"
                "Одноразовый пароль (смените его при первом входе):\n\n"
                "    %s\n\n"
                "============================================================\n\n",
                password.toUtf8().constData());
    std::fflush(stdout);
}