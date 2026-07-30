#include <QTest>
#include <QString>
#include "utils/password_utils.h"

class TestPasswordUtils : public QObject
{
    Q_OBJECT

private slots:
    void testHashAndCheck()
    {
        QString password = "MyTestPass123!";
        QString hash = hashPassword(password);

        QVERIFY(!hash.isEmpty());
        QCOMPARE(hash.count(':'), 2);

        QStringList parts = hash.split(':');
        QCOMPARE(parts[0], QString::number(PBKDF2_ITERATIONS));
        QCOMPARE(parts[1].length(), 32);
        QCOMPARE(parts[2].length(), 64);

        QVERIFY(checkPassword(password, hash));
        QVERIFY(!checkPassword("WrongPass123!", hash));
    }

    void testDeterministicWithSalt()
    {
        QString password = "Test123";
        QString salt = "a1b2c3d4e5f6a7b8";
        QString hash = hashPassword(password, salt);

        QVERIFY(hash.startsWith("100000:a1b2c3d4e5f6a7b8:"));
        QVERIFY(checkPassword(password, hash));
    }

    void testBackwardCompatibilityOldFormat()
    {
        QString password = "MyOldPass1";
        QString hash = QString(QCryptographicHash::hash(
            password.toUtf8(), QCryptographicHash::Sha256).toHex());
        QCOMPARE(hash.length(), 64);
        QVERIFY(checkPassword(password, hash));
    }

    void testBackwardCompatibilitySaltedFormat()
    {
        QString password = "SaltedPass1";
        QString salt = "0123456789abcdef";
        QString hash = salt + QString(QCryptographicHash::hash(
            (salt + password).toUtf8(), QCryptographicHash::Sha256).toHex());
        QCOMPARE(hash.length(), 80);
        QVERIFY(checkPassword(password, hash));
    }

    void testEmptyPassword()
    {
        QString hash = hashPassword("");
        QVERIFY(!hash.isEmpty());
        QVERIFY(checkPassword("", hash));
        QVERIFY(!checkPassword("x", hash));
    }

    void testWrongFormat()
    {
        QVERIFY(!checkPassword("test", "invalid_hash_format"));
        QVERIFY(!checkPassword("test", ""));
    }
};

QTEST_MAIN(TestPasswordUtils)
#include "test_password_utils.moc"
