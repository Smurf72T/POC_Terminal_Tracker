#ifndef BACKUPPANEL_H
#define BACKUPPANEL_H

#include <QObject>
#include <QString>

#include "ops/backupmanager.h"

class QThread;
class BackupWorker;

// Управляет ручным резервным копированием и восстановлением БД:
// создаёт фоновый поток с BackupWorker'ом, следит за занятостью операции.
// Информирует владельца сигналами о завершении.
class BackupPanel : public QObject {
    Q_OBJECT

public:
    explicit BackupPanel(QObject* parent = nullptr);
    ~BackupPanel() override;

    bool isBusy() const;

public slots:
    void performBackup(QWidget* parentWidget);
    void performRestore(QWidget* parentWidget);

signals:
    void backupFinished(const BackupManager::BackupResult& result);
    void restoreFinished(bool ok, const QString& filePath, const QString& error);
    void busyChanged(bool busy);

private:
    void ensureBackupWorker();
    void setBusy(bool busy);

    QThread* m_backupThread = nullptr;
    BackupWorker* m_backupWorker = nullptr;
    bool m_busy = false;
};

#endif // BACKUPPANEL_H