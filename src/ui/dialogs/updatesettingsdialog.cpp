#include "dialogs/updatesettingsdialog.h"

#include "update/updatemanager.h"

#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

UpdateSettingsDialog::UpdateSettingsDialog(UpdateManager* updater, QWidget* parent) : QDialog(parent), m_updater(updater)
{
    setWindowTitle("Обновления");
    setMinimumWidth(460);

    auto* layout = new QVBoxLayout(this);

    m_versionLabel = new QLabel(this);
    m_versionLabel->setText("Текущая версия: " + (m_updater ? m_updater->currentVersion() : QString("-")));

    m_autoCheck = new QCheckBox("Автоматически проверять и устанавливать обновления", this);
    m_autoCheck->setChecked(m_updater && m_updater->autoUpdateEnabled());

    auto* hintLabel = new QLabel("Включено: программа сама проверит наличие новой версии при запуске,\n"
                                 "скачает и установит её, показав актуальную версию.\n"
                                 "Выключено: проверка и обновление — только вручную кнопками ниже.",
                                 this);
    hintLabel->setWordWrap(true);

    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_checkBtn = new QPushButton("Проверить обновление", this);
    m_updateBtn = new QPushButton("Обновить", this);
    m_updateBtn->setEnabled(false);
    m_updateBtn->setToolTip("Скачать и установить найденную версию");

    auto* closeBtn = new QPushButton("Закрыть", this);

    layout->addWidget(m_versionLabel);
    layout->addWidget(m_autoCheck);
    layout->addWidget(hintLabel);
    layout->addWidget(line);
    layout->addWidget(m_statusLabel);
    layout->addStretch();

    auto* btnRow = new QHBoxLayout();
    btnRow->addWidget(m_checkBtn);
    btnRow->addWidget(m_updateBtn);
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);
    layout->addLayout(btnRow);

    connect(m_autoCheck, &QCheckBox::toggled, this, &UpdateSettingsDialog::onAutoToggle);
    connect(m_checkBtn, &QPushButton::clicked, this, &UpdateSettingsDialog::checkForUpdates);
    connect(m_updateBtn, &QPushButton::clicked, this, &UpdateSettingsDialog::downloadAndInstall);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    if (m_updater) {
        connect(m_updater, &UpdateManager::updateAvailable, this, &UpdateSettingsDialog::onUpdateAvailable);
        connect(m_updater, &UpdateManager::noUpdateAvailable, this, &UpdateSettingsDialog::onNoUpdate);
        connect(m_updater, &UpdateManager::checkFailed, this, &UpdateSettingsDialog::onCheckFailed);
        connect(m_updater, &UpdateManager::downloadProgress, this, &UpdateSettingsDialog::onDownloadProgress);
        connect(m_updater, &UpdateManager::downloadFinished, this, &UpdateSettingsDialog::onDownloadFinished);
        connect(m_updater, &UpdateManager::downloadFailed, this, &UpdateSettingsDialog::onDownloadFailed);
    }

    onAutoToggle(m_autoCheck->isChecked());
}

void UpdateSettingsDialog::onAutoToggle(bool checked)
{
    if (m_updater)
        m_updater->setAutoUpdateEnabled(checked);
    m_checkBtn->setVisible(!checked);
    m_updateBtn->setVisible(!checked);
    if (checked) {
        m_updateBtn->setEnabled(false);
        setStatus("Автообновление включено.\n"
                  "Программа проверит и установит новую версию при следующем запуске, "
                  "показав актуальную версию при открытии окна.");
    } else {
        m_updateBtn->setEnabled(false);
        setStatus("Автообновление выключено.\nПроверка наличия обновлений — кнопкой "
                  "«Проверить обновление».");
    }
}

void UpdateSettingsDialog::checkForUpdates()
{
    if (!m_updater || !m_updater->isEnabled()) {
        setStatus("Автообновление не настроено: укажите update.url в config/config.json.", true);
        return;
    }
    m_updateBtn->setEnabled(false);
    setBusy(true);
    setStatus("Проверка обновлений...");
    m_updater->checkForUpdates();
}

void UpdateSettingsDialog::downloadAndInstall()
{
    if (m_newUrl.isEmpty() || !m_updater)
        return;
    if (QMessageBox::question(this, "Обновление",
                              QString("Скачать и установить версию %1?\n"
                                      "Приложение будет закрыто для установки обновления.")
                                  .arg(m_newVersion)) != QMessageBox::Yes)
        return;
    setBusy(true);
    m_updateBtn->setEnabled(false);
    setStatus("Скачивание обновления...");
    m_updater->downloadUpdate(m_newUrl);
}

void UpdateSettingsDialog::onUpdateAvailable(const QString& version, const QString& /*releaseNotes*/, const QString& url)
{
    setBusy(false);
    m_newVersion = version;
    m_newUrl = url;
    m_updateBtn->setEnabled(true);
    setStatus(QString("Доступна новая версия %1.\nНажмите «Обновить», чтобы скачать и установить её.").arg(version));
}

void UpdateSettingsDialog::onNoUpdate()
{
    setBusy(false);
    m_updateBtn->setEnabled(false);
    setStatus(QString("Обновлений нет. Установлена актуальная версия %1.")
                  .arg(m_updater ? m_updater->currentVersion() : QString()));
}

void UpdateSettingsDialog::onCheckFailed(const QString& error)
{
    setBusy(false);
    m_updateBtn->setEnabled(false);
    setStatus(error, true);
}

void UpdateSettingsDialog::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0)
        setStatus(QString("Скачивание обновления: %1 / %2 КБ").arg(bytesReceived / 1024).arg(bytesTotal / 1024));
    else
        setStatus(QString("Скачивание обновления: %1 КБ").arg(bytesReceived / 1024));
}

void UpdateSettingsDialog::onDownloadFinished(const QString& /*filePath*/)
{
    if (!m_updater || m_updater->autoUpdateEnabled())
        return;
    setBusy(true);
    setStatus("Обновление скачано. Установка...");
    m_updater->applyUpdate();
    accept();
}

void UpdateSettingsDialog::onDownloadFailed(const QString& error)
{
    setBusy(false);
    m_updateBtn->setEnabled(true);
    setStatus(error, true);
}

void UpdateSettingsDialog::setBusy(bool busy)
{
    m_checkBtn->setEnabled(!busy);
    m_autoCheck->setEnabled(!busy);
}

void UpdateSettingsDialog::setStatus(const QString& text, bool isError)
{
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(isError ? "color: #E05A5A;" : QString());
}
