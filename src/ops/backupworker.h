#ifndef BACKUPWORKER_H
#define BACKUPWORKER_H

#include <QObject>
#include <QSqlDatabase>
#include <QString>

#include "ops/backupmanager.h"

// Выполняет резервное копирование / восстановление в отдельном потоке,
// чтобы pg_dump/psql (QProcess::waitForFinished) не блокировали UI.
// QSqlDatabase-соединение открывается внутри рабочего потока — соединение,
// созданное в главном потоке, использовать в другом потоке нельзя.
class BackupWorker : public QObject
{
    Q_OBJECT

public:
    struct ConnectionParams {
        QString host;
        int port = 0;
        QString databaseName;
        QString user;
        QString password;
        QString connectOptions;
    };

    // Должен вызываться до moveToThread().
    void setConnectionParams(const ConnectionParams &params);

public slots:
    void createBackup(const QString &filePath, const QString &password);
    void restore(const QString &filePath, const QString &password);

signals:
    void backupFinished(const BackupManager::BackupResult &result);
    void restoreFinished(bool ok, const QString &filePath, const QString &error);

private:
    QSqlDatabase openConnection();

    ConnectionParams m_params;
};

#endif // BACKUPWORKER_H
