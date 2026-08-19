#include "backupmanager.h"
#include "backupmanager_internal.h"

#include <QProcess>
#include <QSqlDatabase>
#include <QTemporaryDir>

bool BackupManager::restoreDatabase(const QSqlDatabase& db, const QString& filePath, const QString& connectionPassword,
                                    const QString& passphrase, QString* error, std::atomic<bool>* cancelRequested)
{
    QString host = db.hostName();
    QString port = QString::number(db.port());
    QString dbname = db.databaseName();
    QString user = db.userName();

    if (cancelRequested && cancelRequested->load()) {
        if (error)
            *error = "Операция отменена пользователем";
        return false;
    }

    // Бэкап может быть зашифрован (маркер POCENC1) — расшифровываем во временный файл.
    QTemporaryDir tmpDir;
    if (!tmpDir.isValid()) {
        if (error)
            *error = "Не удалось создать временную директорию для восстановления";
        return false;
    }
    QString sqlPath = tmpDir.filePath("restore.sql");
    if (!decryptBackupFile(filePath, sqlPath, passphrase, error, cancelRequested))
        return false;

    QStringList args;
    args << QString("--host=%1").arg(host) << QString("--port=%1").arg(port) << QString("--username=%1").arg(user)
         << QString("--dbname=%1").arg(dbname) << QString("--file=%1").arg(sqlPath) << "--single-transaction";

    QProcess process;
    auto env = process.environment();
    env.append(QString("PGPASSWORD=%1").arg(connectionPassword));
    process.setEnvironment(env);
    process.start("psql", args);

    if (!waitForFinishedWithCancel(process, kPsqlTimeoutMs, cancelRequested)) {
        process.kill();
        process.waitForFinished(kKillWaitMs);
        if (error) {
            if (cancelRequested && cancelRequested->load()) {
                *error = "Восстановление отменено пользователем.\n"
                         "Данные могли остаться в прежнем состоянии (restore выполняется "
                         "в одной транзакции).";
            } else {
                *error = QString("psql не завершился за 120 секунд и был остановлен.\n"
                                 "Данные могли остаться в прежнем состоянии (restore выполняется "
                                 "в одной транзакции).\n\n"
                                 "Попробуйте восстановить вручную:\n"
                                 "openssl enc -d -aes-256-cbc -pbkdf2 -iter 100000 -pass pass:<пароль> "
                                 "-in \"%1\" -out restore.sql\n"
                                 "psql -U %2 -d %3 -f restore.sql")
                             .arg(filePath, user, dbname);
            }
        }
        return false;
    }

    QString errorText = process.readAllStandardError();
    int exitCode = process.exitCode();

    // psql возвращает ненулевой код только при реальных ошибках (NOTICE/WARNING не считаются)
    if (exitCode != 0) {
        if (error)
            *error = QString("psql завершился с ошибками (код %1):\n%2").arg(exitCode).arg(errorText.left(2000));
        return false;
    }

    return true;
}