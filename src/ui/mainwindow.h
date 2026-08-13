#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>

#include "ops/backupmanager.h"

namespace Ui {
class MainWindow;
}

class QWidget;
class DashboardView;
class BackupPanel;
class MaintenancePanel;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onActionAbout_triggered();
    void onActionExit_triggered();
    void onActionTerminalHistory_triggered();
    void onActionFreeDevicesReport_triggered();
    void onActionBackup_triggered();
    void onActionRestore_triggered();
    void onActionIntegrityCheck_triggered();
    void onActionOpsLog_triggered();
    void onActionCheckUpdates_triggered();
    void onActionAuditLog_triggered();
    void onActionUserManagement_triggered();
    void onManualBackupFinished(const BackupManager::BackupResult& result);
    void onManualRestoreFinished(bool ok, const QString& filePath, const QString& error);
    void onRecentDocActivated(int docType, int docId);
    void onTopClientActivated(int clientId, const QString& clientName);

private:
    Ui::MainWindow* ui;
    DashboardView* m_dashboard = nullptr;
    BackupPanel* m_backup = nullptr;
    MaintenancePanel* m_maintenance = nullptr;
    QLabel* m_backupStatusLabel = nullptr;
    bool m_darkTheme = true;

    void setupUI();
    void updateStatusBar();
    void openForm(QWidget* form);
    void showGlobalSearch();
};

#endif // MAINWINDOW_H