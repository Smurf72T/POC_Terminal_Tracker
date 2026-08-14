#include "panels/maintenancepanel.h"

#include "database/databasemanager.h"
#include "ops/opslog.h"
#include "ops/opsscheduler.h"
#include "update/updatemanager.h"
#include "utils/logging.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QMessageBox>
#include <QStatusBar>
#include <QTimer>
#include <QUrl>
#include <QWidget>
#include <memory>

MaintenancePanel::MaintenancePanel(const QJsonObject& opsConfig, QStatusBar* statusBar, QLabel* backupStatusLabel,
                                   QWidget* parentWidget, QObject* parent) :
    QObject(parent), m_statusBar(statusBar), m_backupStatusLabel(backupStatusLabel), m_parentWidget(parentWidget)
{
    initOpsLog(opsConfig);
    setupScheduler(opsConfig);
    setupUpdater(opsConfig);
}

MaintenancePanel::~MaintenancePanel() = default;

void MaintenancePanel::initOpsLog(const QJsonObject& opsConfig)
{
    QString logDir = opsConfig["log_directory"].toString().trimmed();
    if (!logDir.isEmpty()) {
        if (QDir::isRelativePath(logDir))
            logDir = QCoreApplication::applicationDirPath() + "/" + logDir;
        OpsLog::instance().setLogDirectory(logDir);
    }
    OpsLog::instance().info(
        QString("Приложение запущено (пользователь: %1, роль: %2)")
            .arg(DatabaseManager::instance().getCurrentUser(), DatabaseManager::instance().getCurrentUserRole()));
}

void MaintenancePanel::setupScheduler(const QJsonObject& opsConfig)
{
    m_scheduler = new OpsScheduler(opsConfig, this);
    connect(m_scheduler, &OpsScheduler::backupFinished, this,
            [this](bool ok, const QString& filePath, const QString& message) {
                m_statusBar->showMessage(message, 15000);
                if (m_backupStatusLabel) {
                    m_backupStatusLabel->setText(QString("Бэкап: %1").arg(ok ? "OK" : "ОШИБКА"));
                    m_backupStatusLabel->setToolTip(message + (ok ? "\nФайл: " + filePath : QString()));
                }
                if (!ok)
                    qCWarning(logApp) << message;
            });
    connect(m_scheduler, &OpsScheduler::integrityFinished, this,
            [this](bool ok, const QString& summary) { m_statusBar->showMessage(summary, 15000); });
}

void MaintenancePanel::setupUpdater(const QJsonObject& opsConfig)
{
    m_updater = new UpdateManager(opsConfig, this);
    // Авторежим: при нахождении новой версии сами скачиваем и устанавливаем её.
    // Ручной режим (переключатель выключен) полностью обслуживает окно
    // UpdateSettingsDialog — здесь эти сигналы игнорируются.
    connect(m_updater, &UpdateManager::updateAvailable, this,
            [this](const QString& version, const QString& /*notes*/, const QString& url) {
                if (!m_updater->autoUpdateEnabled() || url.isEmpty())
                    return;
                m_statusBar->showMessage(QString("Доступна новая версия %1 — скачиваю автоматически...").arg(version),
                                         5000);
                m_updater->downloadUpdate(url);
            });
    connect(m_updater, &UpdateManager::noUpdateAvailable, this, [this]() {
        if (m_updater->autoUpdateEnabled())
            m_statusBar->showMessage(QString("Обновлений нет (версия %1 актуальна)").arg(m_updater->currentVersion()),
                                     8000);
    });
    connect(m_updater, &UpdateManager::checkFailed, this, [this](const QString& error) {
        if (m_updater->isEnabled())
            m_statusBar->showMessage(error, 10000);
        qCWarning(logApp) << error;
    });
    connect(m_updater, &UpdateManager::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (!m_updater->autoUpdateEnabled())
            return;
        if (total > 0)
            m_statusBar->showMessage(
                QString("Скачивание обновления: %1 / %2 КБ").arg(received / 1024).arg(total / 1024));
        else
            m_statusBar->showMessage(QString("Скачивание обновления: %1 КБ").arg(received / 1024));
    });
    connect(m_updater, &UpdateManager::downloadFinished, this, [this](const QString& /*filePath*/) {
        if (!m_updater->autoUpdateEnabled())
            return;
        m_updater->applyUpdate();
    });
    connect(m_updater, &UpdateManager::updateInstallScheduled, this, [this]() {
        QMessageBox::information(m_parentWidget, "Обновление",
                                 "Новая версия скачана и будет установлена.\nПриложение будет перезапущено.");
        QTimer::singleShot(500, qApp, &QCoreApplication::quit);
    });
    connect(m_updater, &UpdateManager::downloadFailed, this, [this](const QString& error) {
        if (m_updater->autoUpdateEnabled())
            QMessageBox::warning(m_parentWidget, "Скачивание обновления", error);
    });
}

void MaintenancePanel::start()
{
    m_scheduler->start();
    m_updater->start();
}

void MaintenancePanel::resetBackupSchedule()
{
    if (m_scheduler)
        m_scheduler->resetLastBackup();
}

void MaintenancePanel::resetIntegritySchedule()
{
    if (m_scheduler)
        m_scheduler->resetIntegrityCheck();
}

void MaintenancePanel::runIntegrityCheck()
{
    if (!m_scheduler)
        return;
    m_statusBar->showMessage("Проверка целостности БД...", 5000);
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(m_scheduler, &OpsScheduler::integrityFinished, this, [this, conn](bool, const QString& summary) {
        QMessageBox::information(m_parentWidget, "Проверка целостности БД", summary);
        disconnect(*conn);
    });
    m_scheduler->runIntegrityCheck();
}

void MaintenancePanel::openOpsLog()
{
    QString path = OpsLog::instance().logFilePath();
    if (!QFile::exists(path)) {
        QMessageBox::information(m_parentWidget, "Журнал операций", "Журнал операций пока пуст.\nПуть: " + path);
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}