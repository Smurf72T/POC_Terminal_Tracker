#ifndef UPDATESETTINGSDIALOG_H
#define UPDATESETTINGSDIALOG_H

#include <QDialog>

class QCheckBox;
class QLabel;
class QPushButton;
class UpdateManager;

// Окно «Обновления» (меню «Сервис → Проверка обновлений»):
// переключатель автообновления + ручная проверка и установка новой версии.
// В авторежиме программа сама проверяет/скачивает/устанавливает обновление
// при запуске; в ручном режиме здесь доступны кнопки «Проверить обновление»
// и «Обновить».
class UpdateSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit UpdateSettingsDialog(UpdateManager* updater, QWidget* parent = nullptr);

private slots:
    void onAutoToggle(bool checked);
    void checkForUpdates();
    void downloadAndInstall();
    void onUpdateAvailable(const QString& version, const QString& releaseNotes, const QString& url);
    void onNoUpdate();
    void onCheckFailed(const QString& error);
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished(const QString& filePath);
    void onDownloadFailed(const QString& error);

private:
    void setBusy(bool busy);
    void setStatus(const QString& text, bool isError = false);

    UpdateManager* m_updater = nullptr;
    QCheckBox* m_autoCheck = nullptr;
    QLabel* m_versionLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_checkBtn = nullptr;
    QPushButton* m_updateBtn = nullptr;
    QString m_newVersion;
    QString m_newUrl;
};

#endif // UPDATESETTINGSDIALOG_H
