#include <QTest>
#include <QString>
#include "utils/validator.h"

class TestValidator : public QObject
{
    Q_OBJECT

private slots:
    void testIMEI_valid()
    {
        QVERIFY(Validator::validateIMEI("490154203237518"));
    }

    void testIMEI_invalid()
    {
        QVERIFY(!Validator::validateIMEI(""));
        QVERIFY(!Validator::validateIMEI("12345"));
        QVERIFY(!Validator::validateIMEI("abcdefghijklmno"));
        QVERIFY(!Validator::validateIMEI("490154203237519"));
    }

    void testINN_10digit()
    {
        QVERIFY(Validator::validateINN("7727563778"));
    }

    void testINN_12digit()
    {
        QVERIFY(Validator::validateINN("123456789047"));
    }

    void testINN_invalid()
    {
        QVERIFY(!Validator::validateINN(""));
        QVERIFY(!Validator::validateINN("12345"));
        QVERIFY(!Validator::validateINN("abcdefghij"));
        QVERIFY(!Validator::validateINN("7727563779"));
    }

    void testLuhn()
    {
        QVERIFY(Validator::checkLuhn("490154203237518"));
        QVERIFY(!Validator::checkLuhn("490154203237519"));
        QVERIFY(!Validator::checkLuhn(""));
        QVERIFY(!Validator::checkLuhn("abc"));
    }

    void testSerialNotEmpty()
    {
        QVERIFY(Validator::validateSerialNotEmpty("ABC123"));
        QVERIFY(!Validator::validateSerialNotEmpty(""));
        QVERIFY(!Validator::validateSerialNotEmpty("  "));
    }
};

QTEST_MAIN(TestValidator)
#include "test_validator.moc"
