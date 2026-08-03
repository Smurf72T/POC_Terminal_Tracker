#include "backupworker.h"

#include "ops/opslog.h"

#include <QSqlError>

void BackupWorker::setConnectionParams(const ConnectionParams &params)
{
    m_params = params;
}

QSqlDatabase BackupWorker::openConnection()
{
    static int counter = 0;
    QString name = QString("backup_worker_%1").arg(++counter);
    QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL", name);
    db.setHostName(m_params.host);
    db.setPort(m_params.port);
    db.setDatabaseName(m_params.databaseName);
    db.setUserName(m_params.user);
    db.setPassword(m_params.password);
    QString options = m_params.connectOptions.trimmed();
    if (options.isEmpty())
        options = "sslmode=prefer";
    db.setConnectOptions(options);

    if (!db.open()) {
        OpsLog::instance().error(QString("BackupWorker: не удалось открыть соединение: %1")
                                     .arg(db.lastError().text()));
    }
    return db;
}

void BackupWorker::createBackup(const QString &filePath, const QString &password)
{
    QString connectionName;
    {
        QSqlDatabase db = openConnection();
        connectionName = db.connectionName();
        if (!db.isOpen()) {
            BackupManager::BackupResult result;
            result.filePath = filePath;
            result.error = "Не удалось открыть соединение с БД в фоновом потоке";
            emit backupFinished(result);
        } else {
            BackupManager::BackupResult result = BackupManager::createBackup(db, filePath, password);
            emit backupFinished(result);
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void BackupWorker::restore(const QString &filePath, const QString &password)
{
    QString connectionName;
    QString error;
    bool ok = false;
    {
        QSqlDatabase db = openConnection();
        connectionName = db.connectionName();
        if (db.isOpen()) {
            ok = BackupManager::restoreDatabase(db, filePath, password, &error);
            db.close();
        } else {
            error = "Не удалось открыть соединение с БД в фоновом потоке";
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    emit restoreFinished(ok, filePath, error);
}
