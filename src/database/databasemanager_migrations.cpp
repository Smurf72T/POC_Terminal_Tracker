#include "databasemanager.h"
#include "utils/logging.h"
#include "utils/password_utils.h"
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QSqlQuery>
#include <QStringConverter>
#include <QTextStream>
#include <cstdio>

bool DatabaseManager::ensureMigrationsTable()
{
    QSqlQuery q(m_database);
    return q.exec("CREATE TABLE IF NOT EXISTS schema_migrations ("
                  "  version VARCHAR(255) PRIMARY KEY,"
                  "  applied_at TIMESTAMP DEFAULT NOW()"
                  ")");
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
    QStringList candidates = {QCoreApplication::applicationDirPath() + "/sql/migrations/",
                              QCoreApplication::applicationDirPath() + "/../sql/migrations/",
                              QCoreApplication::applicationDirPath() + "/../../sql/migrations/"};
    for (const QString& c : candidates) {
        QDir d(c);
        if (d.exists()) {
            migrationsDir = d.absolutePath();
            break;
        }
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
    QStringList candidates = {QCoreApplication::applicationDirPath() + "/sql/migrations/",
                              QCoreApplication::applicationDirPath() + "/../sql/migrations/",
                              QCoreApplication::applicationDirPath() + "/../../sql/migrations/"};
    for (const QString& c : candidates) {
        QDir d(c);
        if (d.exists()) {
            migrationsDir = d.absolutePath();
            break;
        }
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

bool DatabaseManager::runMigrations(const QString& migrationsDir)
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
        qCWarning(logMigration) << "Не удалось взять advisory lock на миграции:" << lockQuery.lastError().text();
        return false;
    }

    bool ok = applyPendingMigrations();

    QSqlQuery unlockQuery(m_database);
    unlockQuery.prepare("SELECT pg_advisory_unlock(:key)");
    unlockQuery.bindValue(":key", kMigrationLockKey);
    if (!unlockQuery.exec()) {
        qCWarning(logMigration) << "Не удалось снять advisory lock:" << unlockQuery.lastError().text();
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

    for (const QString& filePath : pending) {
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
                qCCritical(logDB) << "Не удалось установить пароль admin:" << upd.lastError().text();
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
        qCCritical(logDB) << "Не удалось создать учётную запись admin:" << ins.lastError().text();
    }
}

QString DatabaseManager::generateRandomPassword()
{
    const QString chars = "abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    QString password;
    password.reserve(16);
    QRandomGenerator* rng = QRandomGenerator::global();
    for (int i = 0; i < 16; ++i)
        password += chars.at(rng->bounded(chars.size()));
    return password;
}

void DatabaseManager::printOneTimeAdminPassword(const QString& password)
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
