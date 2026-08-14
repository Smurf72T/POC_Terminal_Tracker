#include <QTest>
#include <QString>

#include "utils/barcodeparser.h"

class TestBarcodeParser : public QObject {
    Q_OBJECT

private slots:
    void plainSerial();
    void plainImei();
    void plainShortGarbage();
    void labeledAllFields();
    void labeledImeiWithSpace();
    void labeledGenericImei();
    void labeledMixedSeparators();
    void gsSeparators();
    void emptyInput();
    void labeledDuplicateImei();
};

void TestBarcodeParser::plainSerial()
{
    BarcodeScan s = BarcodeParser::parse("  ABC-12345  ");
    QCOMPARE(s.serial, QString("ABC-12345"));
    QVERIFY(s.imei1.isEmpty());
    QVERIFY(s.imei2.isEmpty());
    QVERIFY(s.hasData());
}

void TestBarcodeParser::plainImei()
{
    BarcodeScan s = BarcodeParser::parse("356938035643809");
    QCOMPARE(s.imei1, QString("356938035643809"));
    QVERIFY(s.serial.isEmpty());
}

void TestBarcodeParser::plainShortGarbage()
{
    BarcodeScan s = BarcodeParser::parse("ab");
    QVERIFY(!s.hasData());
}

void TestBarcodeParser::labeledAllFields()
{
    BarcodeScan s = BarcodeParser::parse("SN:ABC-123 IMEI1:356938035643809 IMEI2:356938035643821");
    QCOMPARE(s.serial, QString("ABC-123"));
    QCOMPARE(s.imei1, QString("356938035643809"));
    QCOMPARE(s.imei2, QString("356938035643821"));
}

void TestBarcodeParser::labeledImeiWithSpace()
{
    BarcodeScan s = BarcodeParser::parse("SN:ABC-123  IMEI 1 : 356938035643809  IMEI 2 : 356938035643821");
    QCOMPARE(s.serial, QString("ABC-123"));
    QCOMPARE(s.imei1, QString("356938035643809"));
    QCOMPARE(s.imei2, QString("356938035643821"));
}

void TestBarcodeParser::labeledGenericImei()
{
    BarcodeScan s = BarcodeParser::parse("IMEI:356938035643809");
    QCOMPARE(s.imei1, QString("356938035643809"));
    QVERIFY(s.serial.isEmpty());
}

void TestBarcodeParser::labeledMixedSeparators()
{
    // Перевод строк и новые подписи — значения обрезаются до следующего ключа.
    BarcodeScan s = BarcodeParser::parse("SN:ABC-123\nIMEI1:356938035643809\r\nIMEI2:356938035643821");
    QCOMPARE(s.serial, QString("ABC-123"));
    QCOMPARE(s.imei1, QString("356938035643809"));
    QCOMPARE(s.imei2, QString("356938035643821"));
}

void TestBarcodeParser::gsSeparators()
{
    // Эмуляция GS1 payload: функциональные разделители \x1D.
    const QString text = QString("SN") + QChar(0x1D) + QString("ABC-123") + QChar(0x1D) + QString("IMEI1") +
                         QChar(0x1D) + QString("356938035643809");
    BarcodeScan s = BarcodeParser::parse(text);
    QCOMPARE(s.serial, QString("ABC-123"));
    QCOMPARE(s.imei1, QString("356938035643809"));
}

void TestBarcodeParser::emptyInput()
{
    BarcodeScan s = BarcodeParser::parse("");
    QVERIFY(!s.hasData());
    QVERIFY(!BarcodeParser::isImei(""));
    QVERIFY(!BarcodeParser::isImei("12345678901234"));
    QVERIFY(BarcodeParser::isImei("123456789012345"));
}

void TestBarcodeParser::labeledDuplicateImei()
{
    // Два IMEI через общий ключ IMEI — в первый и во второй слот.
    BarcodeScan s = BarcodeParser::parse("SN:ABC-123 IMEI:356938035643809 IMEI:356938035643821");
    QCOMPARE(s.serial, QString("ABC-123"));
    QCOMPARE(s.imei1, QString("356938035643809"));
    QCOMPARE(s.imei2, QString("356938035643821"));
}

QTEST_GUILESS_MAIN(TestBarcodeParser)

#include "test_barcodeparser.moc"