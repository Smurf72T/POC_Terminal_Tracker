#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QSqlQueryModel>
#include <QTimer>
#include <QLineEdit>
#include <QDialog>
#include <QGroupBox>
#include <QMessageBox>
#include <QtCharts/QChartView>
#include <QtCharts/QPieSeries>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>

#include "ops/backupmanager.h"

namespace Ui {
    class MainWindow;
}

class QThread;
class OpsScheduler;

class UpdateManager;

class BackupWorker;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onActionAbout_triggered();
    void onActionExit_triggered();
    void onActionManufacturers_triggered();
    void onActionModels_triggered();
    void onActionClients_triggered();
    void onActionSIMCards_triggered();
    void onActionTerminals_triggered();
    void onActionReceipt_triggered();
    void onActionRental_triggered();
    void onActionReturn_triggered();
    void onActionStatusChange_triggered();
    void onActionPayment_triggered();
    void onActionArchiveReceipt_triggered();
    void onActionArchiveRental_triggered();
    void onActionArchiveReturn_triggered();
    void onActionArchiveStatusChange_triggered();
    void onActionArchivePayment_triggered();
    void onActionTerminalHistory_triggered();
    void onActionFreeDevicesReport_triggered();
    void onActionBulkImport_triggered();
    void onActionBackup_triggered();
    void onActionRestore_triggered();
    void onActionIntegrityCheck_triggered();
    void onActionOpsLog_triggered();
    void onActionCheckUpdates_triggered();
    void onActionExpiryNotifications_triggered();
    void onActionAuditLog_triggered();
    void onActionBatchStatus_triggered();
    void onActionReports_triggered();
    void onActionUserManagement_triggered();
    void onDatabaseDataChanged();
    void onRecentDocDoubleClicked(const QModelIndex &index);
    void onTopClientDoubleClicked(const QModelIndex &index);

private:
    Ui::MainWindow *ui;
    QSqlQueryModel *topClientsModel;
    QSqlQueryModel *recentDocsModel;
    QTimer *refreshTimer;
    bool m_darkTheme = true;
    QChartView *chartStatusView = nullptr;
    QChartView *chartRevenueView = nullptr;
    OpsScheduler *m_opsScheduler = nullptr;
    UpdateManager *m_updater = nullptr;
    QLabel *m_backupStatusLabel = nullptr;

    QThread *m_backupThread = nullptr;
    BackupWorker *m_backupWorker = nullptr;
    bool m_backupBusy = false;

    void setupUI();
    void setupCharts();
    void updateCharts();
    void updateStatusBar();
    void loadCounters();
    void loadTopClients();
    void loadRecentDocuments();
    void updateCounterWidget(QLabel* valueLabel, QLabel* nameLabel, const QString& value, const QString& label, const QString& color);
    void openForm(QWidget *form);
    void openFreeDevicesReport();
    void openClientRentalReport(int clientId, const QString &clientName);
    void openBulkImport();
    void performBackup();
    void performRestore();
    void ensureBackupWorker();
    void showExpiryNotifications();
    void showGlobalSearch();

signals:
    void backupRequested(const QString &filePath, const QString &password);
    void restoreRequested(const QString &filePath, const QString &password);

private slots:
    void onManualBackupFinished(const BackupManager::BackupResult &result);
    void onManualRestoreFinished(bool ok, const QString &filePath, const QString &error);
};

#endif // MAINWINDOW_H
