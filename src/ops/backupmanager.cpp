#include "backupmanager.h"

#include "utils/logging.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTextStream>
#include <QVariant>

namespace {

constexpr int kPgDumpTimeoutMs = 60000;
constexpr int kPsqlTimeoutMs = 120000;
constexpr int kKillWaitMs = 5000;

// Маркер зашифрованного бэкапа (первая строка файла).
// Формат: "POCENC1\n" + шифротекст openssl enc -aes-256-cbc -pbkdf2.
const char kEncMarker[] = "POCENC1\n";

QString findOpenssl()
{
    QString found = QStandardPaths::findExecutable("openssl");
    if (!found.isEmpty())
        return found;
    // Резервные пути (Git for Windows / OpenSSL) — в CI openssl не всегда в PATH.
    const QStringList fallbackPaths = {
#ifdef Q_OS_WIN
        "C:/Program Files/Git/usr/bin/openssl.exe",
        "C:/Program Files/OpenSSL-Win64/bin/openssl.exe",
        "C:/OpenSSL-Win64/bin/openssl.exe",
#endif
        "/usr/bin/openssl",
        "/usr/local/bin/openssl",
    };
    for (const QString &p : fallbackPaths) {
        if (QFileInfo::exists(p))
            return p;
    }
    return QString();
}

bool runOpenssl(const QStringList &args, QString *error)
{
    static const QString kOpenssl = findOpenssl();
    if (kOpenssl.isEmpty()) {
        if (error)
            *error = "openssl не найден — невозможно выполнить шифрование/расшифровку бэкапа";
        return false;
    }
    QProcess process;
    process.start(kOpenssl, args);
    if (!process.waitForFinished(kPgDumpTimeoutMs)) {
        process.kill();
        process.waitForFinished(kKillWaitMs);
        if (error)
            *error = "openssl не завершился за 60 секунд и был остановлен";
        return false;
    }
    QString stderrText = process.readAllStandardError();
    if (process.exitCode() != 0) {
        if (error)
            *error = QString("openssl завершился с ошибкой (код %1):\n%2")
                         .arg(process.exitCode())
                         .arg(stderrText.left(1000));
        return false;
    }
    return true;
}

bool isEncryptedBackup(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    return f.read(qstrlen(kEncMarker)) == QByteArray(kEncMarker);
}

// Шифрует plain-файл в файл с маркером POCENC1 + шифротекстом AES-256-CBC.
bool encryptBackupFile(const QString &plainPath, const QString &outPath,
                       const QString &passphrase, QString *error)
{
    QTemporaryFile cipherFile("enc-XXXXXX.bin");
    if (!cipherFile.open()) {
        if (error)
            *error = "Не удалось создать временный файл для шифрования";
        return false;
    }
    QString cipherPath = cipherFile.fileName();
    cipherFile.close();

    if (!runOpenssl({"enc", "-aes-256-cbc", "-pbkdf2", "-iter", "100000",
                     "-salt", "-pass", "pass:" + passphrase,
                     "-in", plainPath, "-out", cipherPath}, error))
        return false;

    QFile in(cipherPath);
    if (!in.open(QIODevice::ReadOnly)) {
        if (error)
            *error = "Не удалось прочитать временный шифротекст";
        return false;
    }
    QFile out(outPath);
    if (out.exists() && !out.remove()) {
        if (error)
            *error = "Не удалось перезаписать файл: " + outPath;
        return false;
    }
    if (!out.open(QIODevice::WriteOnly)) {
        if (error)
            *error = "Не удалось создать файл: " + outPath;
        return false;
    }
    out.write(kEncMarker, qstrlen(kEncMarker));
    QByteArray buf;
    while (!in.atEnd()) {
        buf = in.read(1024 * 1024);
        out.write(buf);
    }
    out.close();
    in.close();
    return true;
}

// Подготавливает SQL-файл для psql: расшифровывает бэкап (если он с маркером)
// или копирует как есть (обратная совместимость с незашифрованными дампами).
bool decryptBackupFile(const QString &inPath, const QString &outPath,
                       const QString &passphrase, QString *error)
{
    if (!isEncryptedBackup(inPath)) {
        QFile::remove(outPath);
        return QFile::copy(inPath, outPath);
    }
    if (passphrase.isEmpty()) {
        if (error)
            *error = "Файл бэкапа зашифрован, но пароль не предоставлен для расшифровки";
        return false;
    }

    QTemporaryFile bodyFile("dec-XXXXXX.bin");
    if (!bodyFile.open()) {
        if (error)
            *error = "Не удалось создать временный файл для расшифровки";
        return false;
    }
    {
        QFile src(inPath);
        if (!src.open(QIODevice::ReadOnly)) {
            if (error)
                *error = "Не удалось открыть файл бэкапа: " + inPath;
            return false;
        }
        src.read(qstrlen(kEncMarker));
        QByteArray buf;
        while (!src.atEnd()) {
            buf = src.read(1024 * 1024);
            bodyFile.write(buf);
        }
        src.close();
    }
    bodyFile.close();

    QFile::remove(outPath);
    return runOpenssl({"enc", "-d", "-aes-256-cbc", "-pbkdf2", "-iter", "100000",
                       "-pass", "pass:" + passphrase,
                       "-in", bodyFile.fileName(), "-out", outPath}, error);
}

QString escapeSqlLiteral(const QString &value)
{
    QString escaped = value;
    escaped.replace("\\", "\\\\");
    escaped.replace("'", "''");
    return "'" + escaped + "'";
}

QString formatSqlValue(const QVariant &val)
{
    if (val.isNull())
        return "NULL";

    switch (static_cast<QMetaType::Type>(val.typeId())) {
    case QMetaType::QDateTime:
        return escapeSqlLiteral(val.toDateTime().toString(Qt::ISODateWithMs));
    case QMetaType::QDate:
        return escapeSqlLiteral(val.toDate().toString(Qt::ISODate));
    case QMetaType::QTime:
        return escapeSqlLiteral(val.toTime().toString("HH:mm:ss"));
    case QMetaType::Bool:
        return val.toBool() ? "TRUE" : "FALSE";
    case QMetaType::QByteArray:
        return "'\\x" + QString::fromLatin1(val.toByteArray().toHex()) + "'";
    case QMetaType::Double:
        return QString::number(val.toDouble(), 'g', 17);
    case QMetaType::QString:
    case QMetaType::Char:
    case QMetaType::QChar:
    case QMetaType::QStringList:
    case QMetaType::QJsonObject:
    case QMetaType::QJsonArray:
    case QMetaType::QJsonValue:
        return escapeSqlLiteral(val.toString());
    default:
        return val.toString();
    }
}

} // namespace

BackupManager::BackupResult BackupManager::createBackup(const QSqlDatabase &db, const QString &filePath,
                                                         const QString &connectionPassword, const QString &passphrase)
{
    BackupResult result;
    result.filePath = filePath;

    QString host = db.hostName();
    QString port = QString::number(db.port());
    QString dbname = db.databaseName();
    QString user = db.userName();

    // Дамп пишется во временный plain-файл, затем финализируется:
    // при непустом passphrase — шифрование AES-256-CBC (openssl), иначе — копирование как есть.
    QTemporaryDir tmpDir;
    if (!tmpDir.isValid()) {
        result.error = "Не удалось создать временную директорию для бэкапа";
        return result;
    }
    QString plainPath = tmpDir.filePath("backup.sql");

    // --clean/--if-exists добавляют в дамп DROP-инструкции, чтобы дамп можно было
    // восстановить в уже заполненную базу (psql без этого падает на существующих таблицах).
    QStringList args;
    args << "--format=plain"
         << "--encoding=UTF8"
         << "--no-password"
         << "--clean"
         << "--if-exists"
         << QString("--host=%1").arg(host)
         << QString("--port=%1").arg(port)
         << QString("--username=%1").arg(user)
         << QString("--file=%1").arg(plainPath)
         << dbname;

    QProcess process;
    auto env = process.environment();
    env.append(QString("PGPASSWORD=%1").arg(connectionPassword));
    process.setEnvironment(env);
    process.start("pg_dump", args);

    if (!process.waitForFinished(kPgDumpTimeoutMs)) {
        process.kill();
        process.waitForFinished(kKillWaitMs);
        result.error = "pg_dump не завершился за 60 секунд, выполнен fallback-дамп";
        if (createFallbackBackup(db, plainPath, dbname, &result.error))
            result.method = "fallback";
        else
            return result;
    } else {
        QString error = process.readAllStandardError();
        int exitCode = process.exitCode();
        if (exitCode != 0) {
            result.error = QString("pg_dump завершился с ошибкой (код %1):\n%2")
                               .arg(exitCode)
                               .arg(error.left(2000));
            if (createFallbackBackup(db, plainPath, dbname, &result.error))
                result.method = "fallback";
            else
                return result;
        } else {
            result.method = "pg_dump";
        }
    }

    QString finalizeError;
    bool finalized = passphrase.isEmpty()
        ? (QFile::remove(filePath), QFile::copy(plainPath, filePath))
        : encryptBackupFile(plainPath, filePath, passphrase, &finalizeError);
    if (!finalized) {
        result.ok = false;
        result.error = finalizeError.isEmpty()
            ? "Не удалось скопировать файл бэкапа: " + filePath
            : finalizeError;
        return result;
    }

    result.ok = true;
    result.encrypted = !passphrase.isEmpty();
    result.size = QFileInfo(filePath).size();
    return result;
}

bool BackupManager::createFallbackBackup(const QSqlDatabase &db, const QString &filePath, const QString &dbname, QString *error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error)
            *error = "Не удалось создать файл резервной копии: " + filePath;
        return false;
    }

    QTextStream out(&file);

    out << "-- Резервная копия БД " << dbname << "\n";
    out << "-- Создана: " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n";
    out << "-- Восстановление: psql -U <user> -d <dbname> -f <file>\n\n";

    QSqlQuery tableQuery(db);
    if (!tableQuery.exec("SELECT tablename FROM pg_tables WHERE schemaname = 'public' ORDER BY tablename")) {
        if (error)
            *error = "Не удалось получить список таблиц: " + tableQuery.lastError().text();
        qCWarning(logSQL) << "Failed to list tables:" << tableQuery.lastError().text();
        return false;
    }
    QStringList tables;
    while (tableQuery.next())
        tables.append(tableQuery.value(0).toString());

    out << "-- 1. Удаление старых таблиц\n";
    for (const QString &table : tables)
        out << "DROP TABLE IF EXISTS \"" << table << "\" CASCADE;\n";
    out << "\n";

    out << "-- 2. Последовательности (после DROP, т.к. owned-последовательности удаляются вместе с таблицей)\n";
    QSqlQuery seqQuery(db);
    if (seqQuery.exec("SELECT schemaname, sequencename FROM pg_sequences "
                      "WHERE schemaname = 'public' ORDER BY sequencename")) {
        while (seqQuery.next()) {
            QString seq = seqQuery.value(1).toString();
            QSqlQuery stateQuery(db);
            if (stateQuery.exec("SELECT last_value, is_called FROM \"" + seq + "\"") && stateQuery.next()) {
                qint64 lastValue = stateQuery.value(0).toLongLong();
                bool isCalled = stateQuery.value(1).toBool();
                out << "CREATE SEQUENCE IF NOT EXISTS \"" << seq << "\" START 1;\n";
                out << "SELECT setval('" << seq << "', " << lastValue << ", "
                    << (isCalled ? "TRUE" : "FALSE") << ");\n";
            }
        }
    } else {
        qCWarning(logSQL) << "Failed to list sequences:" << seqQuery.lastError().text();
    }
    out << "\n";

    out << "-- 3. Создание таблиц (FK добавляются в конце)\n";
    for (const QString &table : tables) {
        QSqlQuery colQuery(db);
        colQuery.prepare("SELECT column_name, data_type, is_nullable, column_default "
                         "FROM information_schema.columns "
                         "WHERE table_schema = 'public' AND table_name = :tbl "
                         "ORDER BY ordinal_position");
        colQuery.bindValue(":tbl", table);
        if (!colQuery.exec()) {
            qCWarning(logSQL) << "Failed to load columns for table" << table << ":" << colQuery.lastError().text();
            continue;
        }

        QStringList columnDefs;
        while (colQuery.next()) {
            QString name = colQuery.value(0).toString();
            QString type = colQuery.value(1).toString();
            QString nullable = colQuery.value(2).toString();
            QString defaultValue = colQuery.value(3).toString();
            if (type.toUpper().startsWith("INT")) type = "INTEGER";
            QString def = "    \"" + name + "\" " + type +
                          (nullable == "YES" ? " NULL" : " NOT NULL");
            if (!defaultValue.isEmpty())
                def += " DEFAULT " + defaultValue;
            columnDefs.append(def);
        }
        out << "CREATE TABLE \"" << table << "\" (\n"
            << columnDefs.join(",\n") << "\n);\n\n";
    }

    out << "-- 4. Данные\n";
    for (const QString &table : tables) {
        QSqlQuery colQuery(db);
        colQuery.prepare("SELECT column_name FROM information_schema.columns "
                         "WHERE table_schema = 'public' AND table_name = :tbl "
                         "ORDER BY ordinal_position");
        colQuery.bindValue(":tbl", table);
        QStringList columnNames;
        if (colQuery.exec()) {
            while (colQuery.next())
                columnNames.append("\"" + colQuery.value(0).toString() + "\"");
        }
        if (columnNames.isEmpty()) {
            qCWarning(logSQL) << "No columns for table" << table;
            continue;
        }

        QSqlQuery dataQuery(db);
        dataQuery.prepare(QString("SELECT * FROM \"%1\"").arg(table));
        if (!dataQuery.exec()) {
            qCWarning(logSQL) << "Failed to load data from table" << table << ":" << dataQuery.lastError().text();
            continue;
        }

        while (dataQuery.next()) {
            out << "INSERT INTO \"" << table << "\" (" << columnNames.join(", ") << ") VALUES (";
            for (int i = 0; i < dataQuery.record().count(); ++i) {
                if (i > 0) out << ", ";
                out << formatSqlValue(dataQuery.value(i));
            }
            out << ");\n";
        }
        out << "\n";
    }

    out << "-- 5. Функции\n";
    QSqlQuery funcQuery(db);
    if (funcQuery.exec("SELECT pg_get_functiondef(p.oid) FROM pg_proc p "
                       "JOIN pg_namespace n ON n.oid = p.pronamespace "
                       "WHERE n.nspname = 'public' ORDER BY p.oid")) {
        while (funcQuery.next())
            out << funcQuery.value(0).toString() << "\n";
    }
    out << "\n";

    out << "-- 6. Триггеры\n";
    QSqlQuery triggerQuery(db);
    if (triggerQuery.exec("SELECT pg_get_triggerdef(t.oid) FROM pg_trigger t "
                          "JOIN pg_class c ON c.oid = t.tgrelid "
                          "JOIN pg_namespace n ON n.oid = c.relnamespace "
                          "WHERE NOT t.tgisinternal AND n.nspname = 'public'")) {
        while (triggerQuery.next())
            out << triggerQuery.value(0).toString() << ";\n";
    }
    out << "\n";

    out << "-- 7. Индексы (индексы ограничений создаются вместе с ограничениями)\n";
    QSqlQuery indexQuery(db);
    if (indexQuery.exec("SELECT pg_get_indexdef(i.indexrelid) FROM pg_index i "
                        "JOIN pg_class c ON c.oid = i.indexrelid "
                        "JOIN pg_namespace n ON n.oid = c.relnamespace "
                        "LEFT JOIN pg_constraint con ON con.conindid = i.indexrelid "
                        "WHERE n.nspname = 'public' AND con.oid IS NULL "
                        "ORDER BY c.relname")) {
        while (indexQuery.next())
            out << indexQuery.value(0).toString() << ";\n";
    }
    out << "\n";

    out << "-- 8. Ограничения (FK в конце)\n";
    QSqlQuery conQuery(db);
    if (conQuery.exec("SELECT c.conrelid::regclass::text, c.conname, pg_get_constraintdef(c.oid) "
                      "FROM pg_constraint c WHERE c.connamespace = 'public'::regnamespace "
                      "AND c.contype IN ('p','u','c','f') "
                      "ORDER BY (c.contype = 'f'), c.conname")) {
        while (conQuery.next()) {
            out << "ALTER TABLE " << conQuery.value(0).toString()
                << " ADD CONSTRAINT " << conQuery.value(1).toString() << " "
                << conQuery.value(2).toString() << ";\n";
        }
    }
    out << "\n";

    file.close();
    return true;
}

bool BackupManager::restoreDatabase(const QSqlDatabase &db, const QString &filePath,
                                    const QString &connectionPassword, const QString &passphrase, QString *error)
{
    QString host = db.hostName();
    QString port = QString::number(db.port());
    QString dbname = db.databaseName();
    QString user = db.userName();

    // Бэкап может быть зашифрован (маркер POCENC1) — расшифровываем во временный файл.
    QTemporaryDir tmpDir;
    if (!tmpDir.isValid()) {
        if (error)
            *error = "Не удалось создать временную директорию для восстановления";
        return false;
    }
    QString sqlPath = tmpDir.filePath("restore.sql");
    if (!decryptBackupFile(filePath, sqlPath, passphrase, error))
        return false;

    QStringList args;
    args << QString("--host=%1").arg(host)
         << QString("--port=%1").arg(port)
         << QString("--username=%1").arg(user)
         << QString("--dbname=%1").arg(dbname)
         << QString("--file=%1").arg(sqlPath)
         << "--single-transaction";

    QProcess process;
    auto env = process.environment();
    env.append(QString("PGPASSWORD=%1").arg(connectionPassword));
    process.setEnvironment(env);
    process.start("psql", args);

    if (!process.waitForFinished(kPsqlTimeoutMs)) {
        process.kill();
        process.waitForFinished(kKillWaitMs);
        if (error) {
            *error = QString("psql не завершился за 120 секунд и был остановлен.\n"
                             "Данные могли остаться в прежнем состоянии (restore выполняется "
                             "в одной транзакции).\n\n"
                             "Попробуйте восстановить вручную:\n"
                             "openssl enc -d -aes-256-cbc -pbkdf2 -iter 100000 -pass pass:<пароль> "
                             "-in \"%1\" -out restore.sql\n"
                             "psql -U %2 -d %3 -f restore.sql")
                         .arg(filePath, user, dbname);
        }
        return false;
    }

    QString errorText = process.readAllStandardError();
    int exitCode = process.exitCode();

    // psql возвращает ненулевой код только при реальных ошибках (NOTICE/WARNING не считаются)
    if (exitCode != 0) {
        if (error)
            *error = QString("psql завершился с ошибками (код %1):\n%2")
                         .arg(exitCode)
                         .arg(errorText.left(2000));
        return false;
    }

    return true;
}
