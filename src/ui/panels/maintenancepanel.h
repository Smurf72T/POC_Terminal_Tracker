#ifndef MAINTENANCEPANEL_H
#define MAINTENANCEPANEL_H

#include <QObject>
#include <QJsonObject>
#include <QString>

class QLabel;
class QStatusBar;
class QWidget;
class OpsScheduler;
class UpdateManager;

// Эксплуатационные функции главного окна: журнал операций, планировщик
// автобэкапов/проверки целостности (OpsScheduler) и проверка обновлений
// (UpdateManager). Владеет этими компонентами и публикует сообщения
// в статус-бар; владелец лишь вызывает слоты по действиям меню.
class MaintenancePanel : public QObject {
    Q_OBJECT

public:
    explicit MaintenancePanel(const QJsonObject& opsConfig, QStatusBar* statusBar, QLabel* backupStatusLabel,
                              QWidget* parentWidget, QObject* parent = nullptr);
    ~MaintenancePanel() override;

    void start();
    void resetBackupSchedule();
    void resetIntegritySchedule();

public slots:
    void runIntegrityCheck();
    void openOpsLog();
    void checkUpdates();

signals:
    void integrityCheckFinished(const QString& summary);

private:
    void initOpsLog(const QJsonObject& opsConfig);
    void setupScheduler(const QJsonObject& opsConfig);
    void setupUpdater(const QJsonObject& opsConfig);

    QStatusBar* m_statusBar = nullptr;
    QLabel* m_backupStatusLabel = nullptr;
    QWidget* m_parentWidget = nullptr;
    OpsScheduler* m_scheduler = nullptr;
    UpdateManager* m_updater = nullptr;
};

#endif // MAINTENANCEPANEL_H