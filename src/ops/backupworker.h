#ifndef BACKUPWORKER_H
#define BACKUPWORKER_H

#include <QObject>
#include <QSqlDatabase>
#include <QString>

#include <atomic>

#include "ops/backupmanager.h"

class ConnectionPool;
class QThread;

// Выполняет резервное копирование / восстановление в отдельном потоке,
// чтобы pg_dump/psql (QProcess::waitForFinished) не блокировали UI.
// QSqlDatabase-соединение открывается внутри рабочего потока — соединение,
// созданное в главном потоке, использовать в другом потоке нельзя.
// Соединение берётся из ConnectionPool и переиспользуется между операциями.
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

    // Запрашивает отмену текущей операции (проверяется в длинных циклах
    // createFallbackBackup/restoreDatabase). Потокобезопасно.
    void requestCancel();

    BackupWorker();
    ~BackupWorker() override;

public slots:
    void createBackup(const QString &filePath, const QString &connectionPassword, const QString &passphrase);
    void restore(const QString &filePath, const QString &connectionPassword, const QString &passphrase);

signals:
    void backupFinished(const BackupManager::BackupResult &result);
    void restoreFinished(bool ok, const QString &filePath, const QString &error);

private:
    QSqlDatabase openConnection();
    QSqlDatabase createRawConnection();

    ConnectionParams m_params;
    ConnectionPool *m_pool = nullptr;
    std::atomic<bool> m_cancelRequested{false};
};

// Создаёт фоновый поток и worker'а резервного копирования, переносит worker
// в поток и запускает его. Соединение с БД не переносится — параметры снимаются
// в вызывающем (главном) потоке до moveToThread(); внутри рабочего потока
// открывается собственное соединение.
// Возвращает nullptr, если поток уже создан (ничего не делает повторно).
// Подключение сигналов выполняется в вызывающем коде — набор сигналов у
// потребителей различается (MainWindow и OpsScheduler).
BackupWorker *createBackupWorker(QThread *&thread);

#endif // BACKUPWORKER_H
