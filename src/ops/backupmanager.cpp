#include "backupmanager.h"
#include "backupmanager_internal.h"

#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSqlDatabase>
#include <QTemporaryDir>

BackupManager::BackupResult BackupManager::createBackup(const QSqlDatabase& db, const QString& filePath,
                                                        const QString& connectionPassword, const QString& passphrase,
                                                        std::atomic<bool>* cancelRequested)
{
    BackupResult result;
    result.filePath = filePath;

    if (cancelRequested && cancelRequested->load()) {
        result.error = "Операция отменена пользователем";
        return result;
    }

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
         << "--if-exists" << QString("--host=%1").arg(host) << QString("--port=%1").arg(port)
         << QString("--username=%1").arg(user) << QString("--file=%1").arg(plainPath) << dbname;

    QProcess process;
    auto env = process.environment();
    env.append(QString("PGPASSWORD=%1").arg(connectionPassword));
    process.setEnvironment(env);
    process.start("pg_dump", args);

    if (!waitForFinishedWithCancel(process, kPgDumpTimeoutMs, cancelRequested)) {
        process.kill();
        process.waitForFinished(kKillWaitMs);
        if (cancelRequested && cancelRequested->load()) {
            result.error = "Операция отменена пользователем";
            return result;
        }
        result.error = "pg_dump не завершился за 60 секунд, выполнен fallback-дамп";
        if (createFallbackBackup(db, plainPath, dbname, &result.error, cancelRequested))
            result.method = "fallback";
        else
            return result;
    } else {
        QString error = process.readAllStandardError();
        int exitCode = process.exitCode();
        if (exitCode != 0) {
            result.error = QString("pg_dump завершился с ошибкой (код %1):\n%2").arg(exitCode).arg(error.left(2000));
            if (createFallbackBackup(db, plainPath, dbname, &result.error, cancelRequested))
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
                         : encryptBackupFile(plainPath, filePath, passphrase, &finalizeError, cancelRequested);
    if (!finalized) {
        result.ok = false;
        result.error = finalizeError.isEmpty() ? "Не удалось скопировать файл бэкапа: " + filePath : finalizeError;
        return result;
    }

    result.ok = true;
    result.encrypted = !passphrase.isEmpty();
    result.size = QFileInfo(filePath).size();
    return result;
}