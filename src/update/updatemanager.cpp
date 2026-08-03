#include "updatemanager.h"

#include "ops/opslog.h"
#include "update/version.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

UpdateManager::UpdateManager(const QJsonObject &config, QObject *parent)
    : QObject(parent)
{
    QJsonObject update = config["update"].toObject();
    m_url = update["url"].toString().trimmed();
    m_checkOnStartup = update["check_on_startup"].toBool(true);
    m_currentVersion = config["application"].toObject()["version"].toString("1.0.0");

    // Публичный ключ сервера в формате SHA-256 (base64, SubjectPublicKeyInfo).
    // Если задан — сертификат обновляющегося сервера должен ему совпадать,
    // иначе проверка/скачивание обновления отклоняется (защита от MITM).
    m_pinnedSha256 = update["pinned_sha256"].toString().trimmed();
    if (!m_pinnedSha256.isEmpty() && m_pinnedSha256.length() != 44) {
        OpsLog::instance().warning(
            "update.pinned_sha256 имеет некорректный формат (ожидается base64 SHA-256 SPKI, 44 символа) "
            "— проверка отпечатка сертификата отключена");
        m_pinnedSha256.clear();
    }
}

QString UpdateManager::sha256Hex(const QByteArray &data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

QString UpdateManager::spkiSha256Base64(const QSslCertificate &cert)
{
    QByteArray spki = cert.publicKey().toDer();
    return QString::fromLatin1(QCryptographicHash::hash(spki, QCryptographicHash::Sha256).toBase64());
}

bool UpdateManager::certificateMatchesPin(const QSslCertificate &cert) const
{
    return !cert.isNull() && spkiSha256Base64(cert) == m_pinnedSha256;
}

bool UpdateManager::isEnabled() const
{
    return !m_url.isEmpty();
}

bool UpdateManager::startupCheckEnabled() const
{
    return m_checkOnStartup;
}

QString UpdateManager::currentVersion() const
{
    return m_currentVersion;
}

QString UpdateManager::updateUrl() const
{
    return m_url;
}

void UpdateManager::start()
{
    if (isEnabled() && m_checkOnStartup)
        QTimer::singleShot(0, this, &UpdateManager::checkForUpdates);
}

void UpdateManager::checkForUpdates()
{
    if (m_url.isEmpty()) {
        QString msg = "Автообновление не настроено: update.url пуст в config/config.json";
        OpsLog::instance().warning(msg);
        emit checkFailed(msg);
        return;
    }

    emit checkStarted();

    QNetworkRequest request{QUrl(m_url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QString("POC_Terminal_Tracker/%1").arg(m_currentVersion));

    QNetworkReply *reply = m_nam.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { handleManifest(reply); });
}

void UpdateManager::handleManifest(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        QString msg = QString("Не удалось проверить обновления: %1").arg(reply->errorString());
        OpsLog::instance().warning(msg);
        emit checkFailed(msg);
        return;
    }

    if (!m_pinnedSha256.isEmpty()) {
        const QSslCertificate cert = reply->sslConfiguration().peerCertificate();
        if (!certificateMatchesPin(cert)) {
            QString msg = "Обновление отклонено: сертификат сервера не соответствует "
                          "отпечатку update.pinned_sha256 (возможна подмена соединения)";
            OpsLog::instance().error(msg);
            emit checkFailed(msg);
            return;
        }
    }

    QByteArray data = reply->readAll();
    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        QString msg = "Манифест обновлений повреждён: " + parseErr.errorString();
        OpsLog::instance().warning(msg);
        emit checkFailed(msg);
        return;
    }

    QJsonObject manifest = doc.object();
    QString newVersion = manifest["version"].toString().trimmed();
    if (newVersion.isEmpty()) {
        QString msg = "Манифест обновлений не содержит поле version";
        OpsLog::instance().warning(msg);
        emit checkFailed(msg);
        return;
    }

    if (!UpdateUtils::isVersionNewer(newVersion, m_currentVersion)) {
        OpsLog::instance().info(QString("Обновлений нет (текущая версия %1)").arg(m_currentVersion));
        emit noUpdateAvailable();
        emit checkFinished(false);
        return;
    }

    QString notes = manifest["release_notes"].toString();
    QString downloadUrl = manifest["download_url"].toString().trimmed();
    m_expectedSha256 = manifest["sha256"].toString().trimmed().toLower();
    if (!m_expectedSha256.isEmpty() && m_expectedSha256.length() != 64) {
        OpsLog::instance().warning("Манифест обновлений содержит некорректный sha256 — проверка контрольной суммы отключена");
        m_expectedSha256.clear();
    }
    OpsLog::instance().info(QString("Доступна новая версия %1").arg(newVersion));
    emit updateAvailable(newVersion, notes, downloadUrl);
    emit checkFinished(true);
}

void UpdateManager::downloadUpdate(const QString &url)
{
    if (url.isEmpty()) {
        emit downloadFailed("URL для скачивания обновления пуст");
        return;
    }
    if (m_downloadReply) {
        emit downloadFailed("Скачивание уже выполняется");
        return;
    }

    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QString("POC_Terminal_Tracker/%1").arg(m_currentVersion));

    m_downloadReply = m_nam.get(request);

    connect(m_downloadReply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) { emit downloadProgress(received, total); });

    connect(m_downloadReply, &QNetworkReply::finished, this, [this]() {
        QNetworkReply *reply = m_downloadReply;
        m_downloadReply = nullptr;
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            QString msg = "Ошибка скачивания: " + reply->errorString();
            OpsLog::instance().error(msg);
            emit downloadFailed(msg);
            return;
        }

        if (!m_pinnedSha256.isEmpty()) {
            const QSslCertificate cert = reply->sslConfiguration().peerCertificate();
            if (!certificateMatchesPin(cert)) {
                QString msg = "Обновление отклонено: сертификат сервера не соответствует "
                              "отпечатку update.pinned_sha256 (возможна подмена соединения)";
                OpsLog::instance().error(msg);
                emit downloadFailed(msg);
                return;
            }
        }

        QByteArray data = reply->readAll();

        QString expectedSha256 = m_expectedSha256;
        m_expectedSha256.clear();

        if (!expectedSha256.isEmpty()) {
            QString actualSha256 = sha256Hex(data);
            if (actualSha256 != expectedSha256) {
                QString msg = QString("Контрольная сумма обновления не совпала (ожидалось %1, получено %2). "
                                      "Файл отклонён — возможно, он повреждён или подменён.")
                                  .arg(expectedSha256, actualSha256);
                OpsLog::instance().error(msg);
                emit downloadFailed(msg);
                return;
            }
            OpsLog::instance().info("Контрольная сумма обновления (sha256) подтверждена");
        }

        QString fileName = QFileInfo(reply->url().path()).fileName();
        if (fileName.isEmpty())
            fileName = "POC_Terminal_Tracker_update.bin";

        QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        if (dir.isEmpty())
            dir = QDir::homePath();

        QString filePath = dir + "/" + fileName;
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QString msg = "Не удалось сохранить файл обновления: " + filePath;
            OpsLog::instance().error(msg);
            emit downloadFailed(msg);
            return;
        }
        file.write(data);
        file.close();

        OpsLog::instance().info(QString("Скачано обновление: %1 (%2 КБ)")
                                    .arg(filePath)
                                    .arg(data.size() / 1024));
        emit downloadFinished(filePath);
    });
}
