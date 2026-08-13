#include "panels/backuppanel.h"

#include "database/databasemanager.h"
#include "ops/backupworker.h"

#include <QDateTime>
#include <QDialog>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QThread>

BackupPanel::BackupPanel(QObject* parent) : QObject(parent) {}

BackupPanel::~BackupPanel()
{
    if (m_backupThread) {
        m_backupThread->quit();
        if (!m_backupThread->wait(5000)) {
            qWarning() << "Backup worker: не дождались завершения за 5 c, запрашиваем отмену";
            m_backupThread->requestInterruption();
            m_backupWorker->requestCancel();
            m_backupThread->wait(5000);
        }
        delete m_backupWorker;
        delete m_backupThread;
        m_backupWorker = nullptr;
        m_backupThread = nullptr;
    }
}

bool BackupPanel::isBusy() const
{
    return m_busy;
}

void BackupPanel::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged(busy);
}

void BackupPanel::performBackup(QWidget* parentWidget)
{
    if (m_busy) {
        QMessageBox::information(parentWidget, "Бэкап", "Резервное копирование уже выполняется.");
        return;
    }

    QString filePath = QFileDialog::getSaveFileName(
        parentWidget, "Сохранить резервную копию БД",
        QString("backup_poc_%1.sql").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")),
        "SQL файлы (*.sql);;Все файлы (*)");

    if (filePath.isEmpty())
        return;

    QMessageBox::StandardButton reply = QMessageBox::question(parentWidget, "Подтверждение",
                                                              "Для выполнения резервного копирования необходимо:\n"
                                                              "1. Утилита pg_dump должна быть доступна в PATH\n"
                                                              "2. Или указать путь к pg_dump вручную\n\n"
                                                              "Выполнить резервное копирование?",
                                                              QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    QString connectionPassword = db.password();

    bool passOk;
    QString passphrase = QInputDialog::getText(
        parentWidget, "Пароль для шифрования бэкапа",
        "Введите passphrase для шифрования (пусто — без шифрования):", QLineEdit::Password, QString(), &passOk);
    if (!passOk)
        return;

    ensureBackupWorker();
    setBusy(true);
    QMetaObject::invokeMethod(m_backupWorker, "createBackup", Qt::QueuedConnection, Q_ARG(QString, filePath),
                              Q_ARG(QString, connectionPassword), Q_ARG(QString, passphrase));
}

void BackupPanel::performRestore(QWidget* parentWidget)
{
    if (m_busy) {
        QMessageBox::information(parentWidget, "Восстановление", "Операция с БД уже выполняется.");
        return;
    }

    QString filePath = QFileDialog::getOpenFileName(parentWidget, "Выберите файл резервной копии", QString(),
                                                    "SQL файлы (*.sql);;Все файлы (*)");

    if (filePath.isEmpty())
        return;

    QMessageBox::StandardButton reply =
        QMessageBox::warning(parentWidget, "Подтверждение",
                             "Восстановление БД полностью заменит текущие данные!\n\n"
                             "Файл: " +
                                 filePath +
                                 "\n\n"
                                 "Рекомендуется сначала сделать резервную копию текущего состояния.\n\n"
                                 "Продолжить?",
                             QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    QString connectionPassword = db.password();

    bool passOk;
    QString passphrase = QInputDialog::getText(
        parentWidget, "Пароль бэкапа",
        "Введите passphrase бэкапа (пусто — для незашифрованного файла):", QLineEdit::Password, QString(), &passOk);
    if (!passOk)
        return;

    ensureBackupWorker();
    setBusy(true);
    QMetaObject::invokeMethod(m_backupWorker, "restore", Qt::QueuedConnection, Q_ARG(QString, filePath),
                              Q_ARG(QString, connectionPassword), Q_ARG(QString, passphrase));
}

void BackupPanel::ensureBackupWorker()
{
    if (m_backupWorker)
        return;
    m_backupWorker = createBackupWorker(m_backupThread);
    if (!m_backupWorker)
        return;

    connect(m_backupWorker, &BackupWorker::backupFinished, this, [this](const BackupManager::BackupResult& result) {
        setBusy(false);
        emit backupFinished(result);
    });
    connect(m_backupWorker, &BackupWorker::restoreFinished, this,
            [this](bool ok, const QString& filePath, const QString& error) {
                setBusy(false);
                emit restoreFinished(ok, filePath, error);
            });
}