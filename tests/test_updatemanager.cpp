#include <QTest>
#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSignalSpy>
#include <QElapsedTimer>
#include <QUrl>
#include <QCoreApplication>

#include "update/updatemanager.h"

class TestUpdateManager : public QObject {
    Q_OBJECT

private slots:
    void httpsUrlsAccepted();
    void insecureUrlsRejected();
    void localHttpAllowed();
    void emptyUrlRejected();
    void hangingServerTimesOut();
};

void TestUpdateManager::httpsUrlsAccepted()
{
    QVERIFY(UpdateManager::isSecureUpdateUrl("https://example.com/update.json"));
    QVERIFY(UpdateManager::isSecureUpdateUrl("https://updates.poc.local/manifest.json"));
    QVERIFY(UpdateManager::isSecureUpdateUrl("https://example.com:8443/update.json"));
    QVERIFY(UpdateManager::isSecureUpdateUrl("HTTPS://EXAMPLE.COM/update.json"));
}

void TestUpdateManager::insecureUrlsRejected()
{
    QVERIFY(!UpdateManager::isSecureUpdateUrl("http://example.com/update.json"));
    QVERIFY(!UpdateManager::isSecureUpdateUrl("ftp://example.com/update.json"));
    QVERIFY(!UpdateManager::isSecureUpdateUrl("file:///tmp/update.json"));
    QVERIFY(!UpdateManager::isSecureUpdateUrl("not-a-url"));
    QVERIFY(!UpdateManager::isSecureUpdateUrl("update.json"));
}

void TestUpdateManager::localHttpAllowed()
{
    QVERIFY(UpdateManager::isSecureUpdateUrl("http://localhost/update.json"));
    QVERIFY(UpdateManager::isSecureUpdateUrl("http://localhost:8080/update.json"));
    QVERIFY(UpdateManager::isSecureUpdateUrl("http://127.0.0.1/update.json"));
    QVERIFY(UpdateManager::isSecureUpdateUrl("http://[::1]/update.json"));
}

void TestUpdateManager::emptyUrlRejected()
{
    QVERIFY(!UpdateManager::isSecureUpdateUrl(QString()));
}

void TestUpdateManager::hangingServerTimesOut()
{
    // Сервер принимает соединение и не отвечает — проверяем, что checkForUpdates
    // завершается ошибкой таймаута в пределах заданного таймаута.
    QTcpServer server;
    QVERIFY2(server.listen(QHostAddress::LocalHost, 0), qPrintable(server.errorString()));
    QList<QTcpSocket*> openSockets;
    QObject::connect(&server, &QTcpServer::newConnection, [&server, &openSockets]() {
        // Принимаем соединение, но не отвечаем — клиент должен упереться в таймаут.
        while (server.hasPendingConnections()) {
            QTcpSocket* socket = server.nextPendingConnection();
            openSockets.append(socket);
        }
    });

    QJsonObject config;
    config["application"] = QJsonObject{{"version", "1.0.0"}};
    config["update"] =
        QJsonObject{{"url", QString("http://localhost:%1/update.json").arg(server.serverPort())}, {"timeout_ms", 500}};

    UpdateManager um(config);
    QSignalSpy failedSpy(&um, &UpdateManager::checkFailed);

    QElapsedTimer timer;
    timer.start();
    um.checkForUpdates();

    // Ждём сигнал checkFailed (таймаут) в пределах разумного времени.
    QTRY_VERIFY_WITH_TIMEOUT(failedSpy.count() > 0, 5000);

    QVERIFY(timer.elapsed() < 4000);
    QVERIFY2(failedSpy.first().at(0).toString().contains("Таймаут", Qt::CaseInsensitive),
             qPrintable(failedSpy.first().at(0).toString()));
    QCoreApplication::processEvents();
    qDeleteAll(openSockets);
    server.close();
}

QTEST_GUILESS_MAIN(TestUpdateManager)

#include "test_updatemanager.moc"
