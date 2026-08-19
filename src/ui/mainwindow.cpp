#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "database/databasemanager.h"
#include "update/version.h"
#include "panels/backuppanel.h"
#include "panels/maintenancepanel.h"
#include "views/dashboardview.h"
#include "dialogs/manufacturersform.h"
#include "dialogs/modelsform.h"
#include "dialogs/clientsform.h"
#include "dialogs/simcardsform.h"
#include "dialogs/terminalsform.h"
#include "dialogs/receiptform.h"
#include "dialogs/rentalform.h"
#include "dialogs/returnform.h"
#include "dialogs/statuschangeform.h"
#include "dialogs/paymentform.h"
#include "dialogs/archivedocumentsform.h"
#include "dialogs/terminalhistoryform.h"
#include "dialogs/bulkimportform.h"
#include "dialogs/auditlogform.h"
#include "dialogs/expirynotificationsform.h"
#include "dialogs/batchstatusform.h"
#include "dialogs/reportsform.h"
#include "dialogs/usermanagementform.h"
#include "dialogs/globalsearchdialog.h"
#include "dialogs/terminalhistorypickerdialog.h"
#include "dialogs/freedevicesreportdialog.h"
#include "dialogs/clientrentalreportdialog.h"
#include "dialogs/updatesettingsdialog.h"
#include "ops/opslog.h"
#include "utils/logging.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QWidget>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_darkTheme = QSettings("POC", "TerminalTracker").value("darkTheme", true).toBool();
    setupUI();
    updateStatusBar();

    m_dashboard = new DashboardView(ui, ui->centralwidget, this);
    connect(m_dashboard, &DashboardView::recentDocActivated, this, &MainWindow::onRecentDocActivated);
    connect(m_dashboard, &DashboardView::topClientActivated, this, &MainWindow::onTopClientActivated);

    m_backup = new BackupPanel(this);
    connect(m_backup, &BackupPanel::backupFinished, this, &MainWindow::onManualBackupFinished);
    connect(m_backup, &BackupPanel::restoreFinished, this, &MainWindow::onManualRestoreFinished);

    // Эксплуатация: журнал операций + планировщик автобэкапов и проверки целостности
    // + автообновление: проверка манифеста при старте, предложение скачать новую версию.
    QJsonObject opsConfig = DatabaseManager::instance().configObject();
    m_maintenance = new MaintenancePanel(opsConfig, statusBar(), m_backupStatusLabel, this, this);
    m_maintenance->start();

    // Глобальный поиск Ctrl+K
    auto* searchShortcut = new QShortcut(QKeySequence("Ctrl+K"), this);
    connect(searchShortcut, &QShortcut::activated, this, &MainWindow::showGlobalSearch);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupUI()
{
    setWindowTitle("POC Terminal Tracker");
    resize(1200, 800);

    auto* themeBtn = new QPushButton(m_darkTheme ? "☀️ Светлая тема" : "🌙 Тёмная тема", this);
    themeBtn->setFixedHeight(24);
    themeBtn->setStyleSheet("QPushButton { background: transparent; color: #E0E0E0; border: 1px solid #555; "
                            "border-radius: 3px; padding: 2px 8px; font-size: 12px; }"
                            "QPushButton:hover { background: #333; }");
    statusBar()->addPermanentWidget(themeBtn);
    m_backupStatusLabel = new QLabel("Бэкап: —", this);
    m_backupStatusLabel->setToolTip("Статус автоматического резервного копирования");
    statusBar()->addPermanentWidget(m_backupStatusLabel);
    connect(themeBtn, &QPushButton::clicked, this, [this, themeBtn]() {
        m_darkTheme = !m_darkTheme;
        QSettings("POC", "TerminalTracker").setValue("darkTheme", m_darkTheme);
        QFile file(m_darkTheme ? ":/styles/modern.qss" : ":/styles/light.qss");
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString style = QString::fromUtf8(file.readAll());
            qApp->setStyleSheet(style);
            file.close();
        }
        themeBtn->setText(m_darkTheme ? "☀️ Светлая тема" : "🌙 Тёмная тема");
        if (m_dashboard)
            m_dashboard->applyDarkTheme(m_darkTheme);
        themeBtn->setStyleSheet(QString("QPushButton { background: transparent; color: %1; border: 1px solid %2; "
                                        "border-radius: 3px; padding: 2px 8px; font-size: 12px; }"
                                        "QPushButton:hover { background: %3; }")
                                    .arg(m_darkTheme ? "#E0E0E0" : "#212121")
                                    .arg(m_darkTheme ? "#555" : "#999")
                                    .arg(m_darkTheme ? "#333" : "#DDD"));
        updateStatusBar();
    });

    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::onActionAbout_triggered);
    connect(ui->actionExit, &QAction::triggered, this, &MainWindow::onActionExit_triggered);
    connect(ui->actionManufacturers, &QAction::triggered, this, [this]() { openForm(new ManufacturersForm(this)); });
    connect(ui->actionModels, &QAction::triggered, this, [this]() { openForm(new ModelsForm(this)); });
    connect(ui->actionClients, &QAction::triggered, this, [this]() { openForm(new ClientsForm(this)); });
    connect(ui->actionSIMCards, &QAction::triggered, this, [this]() { openForm(new SIMCardsForm(this)); });
    connect(ui->actionTerminals, &QAction::triggered, this, [this]() { openForm(new TerminalsForm(this)); });
    connect(ui->actionReceipt, &QAction::triggered, this, [this]() { openForm(new ReceiptForm(this)); });
    connect(ui->actionRental, &QAction::triggered, this, [this]() { openForm(new RentalForm(this)); });
    connect(ui->actionReturn, &QAction::triggered, this, [this]() { openForm(new ReturnForm(this)); });
    connect(ui->actionStatusChange, &QAction::triggered, this, [this]() { openForm(new StatusChangeForm(this)); });
    connect(ui->actionPayment, &QAction::triggered, this, [this]() { openForm(new PaymentForm(this)); });
    connect(ui->actionArchiveReceipt, &QAction::triggered, this,
            [this]() { openForm(new ArchiveDocumentsForm(1, this)); });
    connect(ui->actionArchiveRental, &QAction::triggered, this,
            [this]() { openForm(new ArchiveDocumentsForm(2, this)); });
    connect(ui->actionArchiveReturn, &QAction::triggered, this,
            [this]() { openForm(new ArchiveDocumentsForm(3, this)); });
    connect(ui->actionArchiveStatusChange, &QAction::triggered, this,
            [this]() { openForm(new ArchiveDocumentsForm(5, this)); });
    connect(ui->actionArchivePayment, &QAction::triggered, this,
            [this]() { openForm(new ArchiveDocumentsForm(4, this)); });
    connect(ui->actionTerminalHistory, &QAction::triggered, this, &MainWindow::onActionTerminalHistory_triggered);
    connect(ui->actionFreeDevicesReport, &QAction::triggered, this, &MainWindow::onActionFreeDevicesReport_triggered);
    connect(ui->actionBulkImport, &QAction::triggered, this, [this]() { openForm(new BulkImportForm(this)); });
    connect(ui->actionBackup, &QAction::triggered, this, &MainWindow::onActionBackup_triggered);
    connect(ui->actionRestore, &QAction::triggered, this, &MainWindow::onActionRestore_triggered);
    connect(ui->actionIntegrityCheck, &QAction::triggered, this, &MainWindow::onActionIntegrityCheck_triggered);
    connect(ui->actionOpsLog, &QAction::triggered, this, &MainWindow::onActionOpsLog_triggered);
    connect(ui->actionCheckUpdates, &QAction::triggered, this, &MainWindow::onActionCheckUpdates_triggered);
    connect(ui->actionExpiryNotifications, &QAction::triggered, this,
            [this]() { openForm(new ExpiryNotificationsForm(this)); });
    connect(ui->actionAuditLog, &QAction::triggered, this, &MainWindow::onActionAuditLog_triggered);
    connect(ui->actionBatchStatus, &QAction::triggered, this, [this]() { openForm(new BatchStatusForm(this)); });
    connect(ui->actionReports, &QAction::triggered, this, [this]() { openForm(new ReportsForm(this)); });
    connect(ui->actionUserManagement, &QAction::triggered, this, &MainWindow::onActionUserManagement_triggered);
    connect(ui->actionGlobalSearch, &QAction::triggered, this, &MainWindow::showGlobalSearch);

    // Скрываем admin-only пункты меню для обычных пользователей
    if (!DatabaseManager::instance().isCurrentUserAdmin()) {
        ui->actionUserManagement->setVisible(false);
        ui->actionAuditLog->setVisible(false);
        ui->actionBackup->setVisible(false);
        ui->actionRestore->setVisible(false);
        ui->actionIntegrityCheck->setVisible(false);
        ui->actionOpsLog->setVisible(false);
    }
}

void MainWindow::updateStatusBar()
{
    QString version = UpdateUtils::appVersion(
        DatabaseManager::instance().configObject()["application"].toObject()["version"].toString("1.0.0"));
    QString statusText = QString("Версия %1 | Подключено к БД: ").arg(version) +
                         (DatabaseManager::instance().isConnected() ? "Да" : "Нет") + " | " +
                         QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss");
    statusBar()->showMessage(statusText);
}

void MainWindow::openForm(QWidget* form)
{
    form->setAttribute(Qt::WA_DeleteOnClose);
    form->setWindowModality(Qt::WindowModal);
    form->show();
    form->activateWindow();

    // Центрирование относительно главного окна (после show, когда размеры известны)
    QRect mainRect = this->geometry();
    int x = mainRect.x() + (mainRect.width() - form->width()) / 2;
    int y = mainRect.y() + (mainRect.height() - form->height()) / 2;
    form->move(x, y);
}

void MainWindow::showGlobalSearch()
{
    GlobalSearchDialog dialog(this);
    connect(&dialog, &GlobalSearchDialog::itemActivated, this, [this](int type, int id) {
        switch (type) {
            case 1:
                openForm(new TerminalsForm(this));
                break;
            case 2:
                openForm(new ClientsForm(this));
                break;
            case 3:
                openForm(new SIMCardsForm(this));
                break;
            case 4:
                openForm(new ModelsForm(this));
                break;
            case 5:
                openForm(new ManufacturersForm(this));
                break;
            default:
                break;
        }
    });
    dialog.exec();
}

void MainWindow::onRecentDocActivated(int docType, int docId)
{
    if (docType == 1) {
        ReceiptForm form(this);
        form.loadForEdit(docId);
        form.exec();
    } else if (docType == 2) {
        RentalForm form(this);
        form.loadForEdit(docId);
        form.exec();
    } else if (docType == 3) {
        ReturnForm form(this);
        form.loadForEdit(docId);
        form.exec();
    } else if (docType == 5) {
        StatusChangeForm form(this);
        form.loadForEdit(docId);
        form.exec();
    }
}

void MainWindow::onTopClientActivated(int clientId, const QString& clientName)
{
    auto* dialog = new ClientRentalReportDialog(clientId, clientName, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->open();
}

void MainWindow::onActionAbout_triggered()
{
    QString version = UpdateUtils::appVersion(
        DatabaseManager::instance().configObject()["application"].toObject()["version"].toString("1.0.0"));
    QMessageBox::about(this, "О программе",
                       "POC Terminal Tracker\n"
                       "Версия " +
                           version +
                           "\n\n"
                           "Система учёта POC-терминалов и SIM-карт\n\n"
                           "Связь с разработчиком: ipdoc72@yandex.ru");
}

void MainWindow::onActionExit_triggered()
{
    DatabaseManager::instance().close();
    QCoreApplication::quit();
}

void MainWindow::onActionTerminalHistory_triggered()
{
    TerminalHistoryPickerDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted)
        openForm(new TerminalHistoryForm(dialog.terminalId(), dialog.serialNumber(), this));
}

void MainWindow::onActionFreeDevicesReport_triggered()
{
    auto* dialog = new FreeDevicesReportDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->open();
}

void MainWindow::onActionBackup_triggered()
{
    if (!DatabaseManager::instance().isCurrentUserAdmin()) {
        QMessageBox::warning(this, "Доступ запрещён", "Только администратор может создавать резервные копии.");
        return;
    }
    m_backup->performBackup(this);
}

void MainWindow::onActionRestore_triggered()
{
    if (!DatabaseManager::instance().isCurrentUserAdmin()) {
        QMessageBox::warning(this, "Доступ запрещён", "Только администратор может восстанавливать базу данных.");
        return;
    }
    m_backup->performRestore(this);
}

void MainWindow::onActionIntegrityCheck_triggered()
{
    if (!DatabaseManager::instance().isCurrentUserAdmin()) {
        QMessageBox::warning(this, "Доступ запрещён", "Только администратор может запускать проверку целостности БД.");
        return;
    }
    m_maintenance->runIntegrityCheck();
}

void MainWindow::onActionOpsLog_triggered()
{
    if (!DatabaseManager::instance().isCurrentUserAdmin()) {
        QMessageBox::warning(this, "Доступ запрещён", "Только администратор может открывать журнал операций.");
        return;
    }
    m_maintenance->openOpsLog();
}

void MainWindow::onActionCheckUpdates_triggered()
{
    if (!m_maintenance->updater())
        return;
    UpdateSettingsDialog dialog(m_maintenance->updater(), this);
    dialog.exec();
}

void MainWindow::onActionAuditLog_triggered()
{
    if (!DatabaseManager::instance().isCurrentUserAdmin()) {
        QMessageBox::warning(this, "Доступ запрещён", "Только администратор может просматривать журнал аудита.");
        return;
    }
    openForm(new AuditLogForm(this));
}

void MainWindow::onActionUserManagement_triggered()
{
    if (!DatabaseManager::instance().isCurrentUserAdmin()) {
        QMessageBox::warning(this, "Доступ запрещён", "Только администратор может управлять пользователями.");
        return;
    }
    openForm(new UserManagementForm(this));
}

void MainWindow::onManualBackupFinished(const BackupManager::BackupResult& result)
{
    statusBar()->clearMessage();
    m_maintenance->resetBackupSchedule();

    if (result.ok) {
        QString msg = QString("Резервная копия создана:\n%1\nРазмер: %2 KB")
                          .arg(result.filePath)
                          .arg(QString::number(result.size / 1024));
        if (result.method == "fallback")
            msg += "\nМетод: SQL-дамп через Qt SQL (fallback)";
        QMessageBox::information(this, "Успех", msg);
        OpsLog::instance().info(QString("Ручной бэкап создан (метод: %1): %2, размер %3 KB")
                                    .arg(result.method, result.filePath)
                                    .arg(QString::number(result.size / 1024)));
    } else {
        QMessageBox::critical(this, "Ошибка резервного копирования", result.error);
        OpsLog::instance().error(QString("Ручной бэкап не удался: %1").arg(result.error));
    }
}

void MainWindow::onManualRestoreFinished(bool ok, const QString& filePath, const QString& error)
{
    statusBar()->clearMessage();
    m_maintenance->resetIntegritySchedule();

    if (!ok) {
        QMessageBox::critical(this, "Ошибка восстановления", error);
        OpsLog::instance().error(QString("Восстановление БД не удалось: %1").arg(error));
        return;
    }

    // Обновляем данные на дашборде
    if (m_dashboard)
        m_dashboard->refreshAll();

    QMessageBox::information(this, "Успех",
                             "База данных восстановлена из резервной копии.\n"
                             "Файл: " +
                                 QFileInfo(filePath).fileName());
    OpsLog::instance().info(QString("БД восстановлена из резервной копии: %1").arg(filePath));
}