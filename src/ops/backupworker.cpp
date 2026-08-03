#include "backupworker.h"

#include "database/connectionpool.h"
#include "ops/opslog.h"

#include <QSqlError>
#include <QUuid>

void BackupWorker::setConnectionParams(const ConnectionParams &params)
{
    m_params = params;
}

BackupWorker::~BackupWorker()
{
    delete m_pool;
}

QSqlDatabase BackupWorker::openConnection()
{
    if (!m_pool) {
        m_pool = new ConnectionPool(
            [this]() { return createRawConnection(); }, 1);
    }
    return m_pool->acquire();
}

QSqlDatabase BackupWorker::createRawConnection()
{
    QString name = QString("backup_worker_%1")
                       .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
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
    QSqlDatabase db = openConnection();
    BackupManager::BackupResult result;
    if (db.isOpen()) {
        result = BackupManager::createBackup(db, filePath, password);
    } else {
        result.filePath = filePath;
        result.error = "Не удалось открыть соединение с БД в фоновом потоке";
    }

    if (m_pool)
        m_pool->release(db);
    emit backupFinished(result);
}

void BackupWorker::restore(const QString &filePath, const QString &password)
{
    QString error;
    bool ok = false;
    QSqlDatabase db = openConnection();
    if (db.isOpen()) {
        ok = BackupManager::restoreDatabase(db, filePath, password, &error);
    } else {
        error = "Не удалось открыть соединение с БД в фоновом потоке";
    }

    if (m_pool)
        m_pool->release(db);
    emit restoreFinished(ok, filePath, error);
}
