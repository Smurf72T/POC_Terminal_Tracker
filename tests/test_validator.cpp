#include <QTest>
#include <QString>
#include "utils/validator.h"

class TestValidator : public QObject
{
    Q_OBJECT

private slots:
    void testValidateIMEI_valid()
    {
        QVERIFY(Validator::validateIMEI("990000862471854"));
        QVERIFY(Validator::validateIMEI("353142441234567"));
        QVERIFY(Validator::validateIMEI("000000000000000"));
    }

    void testValidateIMEI_invalid()
    {
        QVERIFY(!Validator::validateIMEI(""));
        QVERIFY(!Validator::validateIMEI("12345678901234"));   // 14 digits
        QVERIFY(!Validator::validateIMEI("1234567890123456")); // 16 digits
        QVERIFY(!Validator::validateIMEI("abcd12345678901"));  // letters
        // hyphens are stripped, so "99-0000862471854" becomes valid 15 digits
    }

    void testValidateINEI_stripsNonDigits()
    {
        QVERIFY(Validator::validateIMEI("99-0000862471854"));  // hyphens stripped → 15 digits
        QVERIFY(Validator::validateIMEI("99 0000862471854"));  // spaces stripped
    }

    void testValidateINN_10digit()
    {
        QVERIFY(Validator::validateINN("7707083893"));
        QVERIFY(Validator::validateINN("1234567894"));
    }

    void testValidateINN_12digit()
    {
        QVERIFY(Validator::validateINN("770708389324"));
    }

    void testValidateINN_invalid()
    {
        QVERIFY(!Validator::validateINN(""));
        QVERIFY(!Validator::validateINN("12345"));              // too short
        QVERIFY(!Validator::validateINN("12345678901"));        // 11 digits — neither 10 nor 12
        QVERIFY(!Validator::validateINN("7707083890"));         // wrong checksum (10-digit)
        QVERIFY(!Validator::validateINN("770708389325"));       // wrong checksum (12-digit)
        QVERIFY(!Validator::validateINN("abcdefghij"));         // letters
    }

    void testValidateSerialNotEmpty_valid()
    {
        QVERIFY(Validator::validateSerialNotEmpty("ABC123"));
        QVERIFY(Validator::validateSerialNotEmpty("SN-001"));
        QVERIFY(Validator::validateSerialNotEmpty("   ABC   "));
    }

    void testValidateSerialNotEmpty_invalid()
    {
        QVERIFY(!Validator::validateSerialNotEmpty(""));
        QVERIFY(!Validator::validateSerialNotEmpty("   "));
        QVERIFY(!Validator::validateSerialNotEmpty("AB"));     // 2 chars
        QVERIFY(!Validator::validateSerialNotEmpty("a"));      // 1 char
        QVERIFY(!Validator::validateSerialNotEmpty("  A  "));  // 1 char after trim
    }

    void testCheckLuhn_valid()
    {
        // Known valid Luhn numbers:
        QVERIFY(Validator::checkLuhn("79927398713")); // classic test
        QVERIFY(Validator::checkLuhn("4532015112830366")); // Visa test
        QVERIFY(Validator::checkLuhn("0"));
    }

    void testCheckLuhn_invalid()
    {
        QVERIFY(!Validator::checkLuhn(""));
        QVERIFY(!Validator::checkLuhn("79927398710")); // wrong check digit
        QVERIFY(!Validator::checkLuhn("123456789"));
        QVERIFY(!Validator::checkLuhn("abcdef"));
    }

    void testValidateINNChecksum_10digit()
    {
        QVERIFY(Validator::validateINNChecksum("7707083893"));
        QVERIFY(Validator::validateINNChecksum("1234567894"));
        QVERIFY(!Validator::validateINNChecksum("7707083890"));
    }

    void testValidateINNChecksum_12digit()
    {
        QVERIFY(Validator::validateINNChecksum("770708389324"));
        QVERIFY(!Validator::validateINNChecksum("770708389325"));
    }

    void testCreateValidators()
    {
        QRegularExpression imeiRe = Validator::createIMEIValidator();
        QVERIFY(imeiRe.isValid());
        QVERIFY(imeiRe.match("990000862471854").hasMatch());
        QVERIFY(!imeiRe.match("invalid").hasMatch());

        QRegularExpression innRe = Validator::createINNValidator();
        QVERIFY(innRe.isValid());
        QVERIFY(innRe.match("7707083893").hasMatch());
        QVERIFY(innRe.match("770708389324").hasMatch());
        QVERIFY(!innRe.match("12345").hasMatch());

        QRegularExpression serialRe = Validator::createSerialValidator();
        QVERIFY(serialRe.isValid());
        QVERIFY(serialRe.match("ABC-123").hasMatch());
        QVERIFY(!serialRe.match("").hasMatch());
    }
};

QTEST_MAIN(TestValidator)
#include "test_validator.moc"
