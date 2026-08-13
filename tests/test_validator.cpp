#include <QTest>
#include <QString>
#include <QRandomGenerator>
#include "utils/validator.h"
#include "utils/password_utils.h"

class TestValidator : public QObject {
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
        QVERIFY(Validator::validateIMEI("99-0000862471854")); // hyphens stripped → 15 digits
        QVERIFY(Validator::validateIMEI("99 0000862471854")); // spaces stripped
    }

    void testValidateINN_10digit()
    {
        QVERIFY(Validator::validateINN("7707083893"));
        QVERIFY(Validator::validateINN("1234567894"));
    }

    void testValidateINN_12digit() { QVERIFY(Validator::validateINN("770708389324")); }

    void testValidateINN_invalid()
    {
        QVERIFY(!Validator::validateINN(""));
        QVERIFY(!Validator::validateINN("12345"));        // too short
        QVERIFY(!Validator::validateINN("12345678901"));  // 11 digits — neither 10 nor 12
        QVERIFY(!Validator::validateINN("7707083890"));   // wrong checksum (10-digit)
        QVERIFY(!Validator::validateINN("770708389325")); // wrong checksum (12-digit)
        QVERIFY(!Validator::validateINN("abcdefghij"));   // letters
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
        QVERIFY(!Validator::validateSerialNotEmpty("AB"));    // 2 chars
        QVERIFY(!Validator::validateSerialNotEmpty("a"));     // 1 char
        QVERIFY(!Validator::validateSerialNotEmpty("  A  ")); // 1 char after trim
    }

    void testCheckLuhn_valid()
    {
        // Known valid Luhn numbers:
        QVERIFY(Validator::checkLuhn("79927398713"));      // classic test
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

    void fuzz_random_validator_inputs()
    {
        // Рандомизированный (детерминированный seed) прогон по всем чистым функциям
        // валидатора: случайные строки не должны приводить к краху/зависанию,
        // регулярные выражения остаются валидными.
        QRandomGenerator rng(0xC0FFEEu);
        const int rounds = 20000;
        for (int i = 0; i < rounds; ++i) {
            QString s;
            const int len = int(rng.bounded(64));
            for (int j = 0; j < len; ++j) {
                switch (rng.bounded(4)) {
                    case 0:
                        s += QChar('0' + rng.bounded(10));
                        break;
                    case 1:
                        s += QChar('A' + rng.bounded(26));
                        break;
                    case 2:
                        s += QChar(0x0410 + rng.bounded(32));
                        break; // кириллица
                    default:
                        s += QChar(0x20 + rng.bounded(0x5F));
                        break; // печатный ASCII
                }
            }
            (void)Validator::validateIMEI(s);
            (void)Validator::validateINN(s);
            (void)Validator::validateSerialNotEmpty(s);
            (void)Validator::checkLuhn(s);
            (void)Validator::validateINNChecksum(s);
            (void)Validator::createIMEIValidator().match(s);
            (void)Validator::createINNValidator().match(s);
            (void)Validator::createSerialValidator().match(s);
        }
    }

    void fuzz_password_hashing()
    {
        // PBKDF2 (100k итераций) — дорогой, поэтому раундов немного.
        QRandomGenerator rng(0xBEEFu);
        const int rounds = 15;
        for (int i = 0; i < rounds; ++i) {
            QString s;
            const int len = int(rng.bounded(1, 32));
            for (int j = 0; j < len; ++j) {
                switch (rng.bounded(3)) {
                    case 0:
                        s += QChar('0' + rng.bounded(10));
                        break;
                    case 1:
                        s += QChar('A' + rng.bounded(26));
                        break;
                    default:
                        s += QChar(0x20 + rng.bounded(0x5F));
                        break;
                }
            }
            QString hash = hashPassword(s);
            QVERIFY2(hash.count(':') == 2, qPrintable("PBKDF2-хеш должен быть в формате iter:salt:hash"));
            QVERIFY(checkPassword(s, hash));
            QVERIFY(!checkPassword(s + "x", hash));
        }
        // Мусор в хранимом хеше отклоняется без краха
        QVERIFY(!checkPassword("password", ""));
        QVERIFY(!checkPassword("password", "::"));
        QVERIFY(!checkPassword("password", "garbage"));
        QVERIFY(!checkPassword("password", QString(64, 'z')));
    }
};

QTEST_MAIN(TestValidator)
#include "test_validator.moc"
