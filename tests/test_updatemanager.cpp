#include <QTest>
#include <QString>

#include "update/updatemanager.h"

class TestUpdateManager : public QObject
{
    Q_OBJECT

private slots:
    void httpsUrlsAccepted();
    void insecureUrlsRejected();
    void localHttpAllowed();
    void emptyUrlRejected();
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

QTEST_GUILESS_MAIN(TestUpdateManager)

#include "test_updatemanager.moc"
