#ifndef UPDATEMANAGER_H
#define UPDATEMANAGER_H

#include <QObject>
#include <QJsonObject>
#include <QNetworkAccessManager>

class QNetworkReply;

class UpdateManager : public QObject
{
    Q_OBJECT

public:
    explicit UpdateManager(const QJsonObject &config, QObject *parent = nullptr);

    bool isEnabled() const;
    bool startupCheckEnabled() const;
    QString currentVersion() const;
    QString updateUrl() const;

    void start();

public slots:
    void checkForUpdates();
    void downloadUpdate(const QString &url);

signals:
    void checkStarted();
    void checkFinished(bool updateAvailable);
    void updateAvailable(const QString &version, const QString &releaseNotes, const QString &downloadUrl);
    void noUpdateAvailable();
    void checkFailed(const QString &error);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void downloadFinished(const QString &filePath);
    void downloadFailed(const QString &error);

private:
    void handleManifest(QNetworkReply *reply);

    QNetworkAccessManager m_nam;
    QString m_url;
    bool m_checkOnStartup = true;
    QString m_currentVersion = "1.0.0";
    QNetworkReply *m_downloadReply = nullptr;
};

#endif // UPDATEMANAGER_H
