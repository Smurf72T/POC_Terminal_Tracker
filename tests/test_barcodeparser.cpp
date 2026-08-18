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
    void applyScanFullSequence();
    void applyScanSecondImeiFillsSlot2();
    void applyScanLabeledImei2();
    void applyScanImeiFirstNoSerial();
    void applyScanOneScanAllFields();
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

void TestBarcodeParser::applyScanFullSequence()
{
    // Полная последовательность: SN → IMEI1 → IMEI2 → следующий SN...
    QStringList s, i1, i2;
    QVERIFY(BarcodeParser::applyScan(s, i1, i2, BarcodeParser::parse("ABC-001")));
    QCOMPARE(s, QStringList({"ABC-001"}));
    QVERIFY(s.size() == 1);

    // Голый IMEI (15 цифр) — в первый слот текущего комплекта.
    QVERIFY(BarcodeParser::applyScan(s, i1, i2, BarcodeParser::parse("356938035643809")));
    QCOMPARE(i1, QStringList({"356938035643809"}));
    QVERIFY(s.size() == 1);

    // Второй голый IMEI — в IMEI 2 того же комплекта, а НЕ новой строкой.
    QVERIFY(BarcodeParser::applyScan(s, i1, i2, BarcodeParser::parse("356938035643821")));
    QCOMPARE(s, QStringList({"ABC-001"}));
    QCOMPARE(i1, QStringList({"356938035643809"}));
    QCOMPARE(i2, QStringList({"356938035643821"}));

    // Следующий SN открывает новый комплект.
    QVERIFY(BarcodeParser::applyScan(s, i1, i2, BarcodeParser::parse("ABC-002")));
    QCOMPARE(s, QStringList({"ABC-001", "ABC-002"}));
    QCOMPARE(i2, QStringList({"356938035643821", ""}));
}

void TestBarcodeParser::applyScanSecondImeiFillsSlot2()
{
    // Регресс-тест: второй отсканированный IMEI должен попасть в IMEI 2
    // текущего комплекта, а не создать новую строку с IMEI 1.
    QStringList s, i1, i2;
    BarcodeParser::applyScan(s, i1, i2, BarcodeParser::parse("ABC-123"));
    BarcodeParser::applyScan(s, i1, i2, BarcodeParser::parse("111111111111111"));
    BarcodeParser::applyScan(s, i1, i2, BarcodeParser::parse("222222222222222"));
    QCOMPARE(s.size(), 1);
    QCOMPARE(i1.size(), 1);
    QCOMPARE(i2.size(), 1);
    QCOMPARE(i1.at(0), QString("111111111111111"));
    QCOMPARE(i2.at(0), QString("222222222222222"));
}

void TestBarcodeParser::applyScanLabeledImei2()
{
    // Подписанный IMEI 2 сразу попадает в слот IMEI 2 текущего комплекта.
    QStringList s, i1, i2;
    BarcodeParser::applyScan(s, i1, i2, BarcodeParser::parse("ABC-123"));
    BarcodeParser::applyScan(s, i1, i2, BarcodeParser::parse("IMEI2:222222222222222"));
    QCOMPARE(s.size(), 1);
    QCOMPARE(i1.at(0), QString(""));
    QCOMPARE(i2.at(0), QString("222222222222222"));
}

void TestBarcodeParser::applyScanImeiFirstNoSerial()
{
    // IMEI до серийника: создаётся строка без SN, затем SN в неё дописывается.
    QStringList s, i1, i2;
    BarcodeParser::applyScan(s, i1, i2, BarcodeParser::parse("356938035643809"));
    QCOMPARE(s, QStringList({""}));
    QCOMPARE(i1, QStringList({"356938035643809"}));

    BarcodeParser::applyScan(s, i1, i2, BarcodeParser::parse("ABC-001"));
    QCOMPARE(s, QStringList({"ABC-001"}));
    QCOMPARE(i1, QStringList({"356938035643809"}));
}

void TestBarcodeParser::applyScanOneScanAllFields()
{
    // Подписанный скан со всеми полями — один комплект целиком.
    QStringList s, i1, i2;
    BarcodeParser::applyScan(s, i1, i2,
                             BarcodeParser::parse("SN:ABC-001 IMEI1:111111111111111 IMEI2:222222222222222"));
    QCOMPARE(s, QStringList({"ABC-001"}));
    QCOMPARE(i1, QStringList({"111111111111111"}));
    QCOMPARE(i2, QStringList({"222222222222222"}));
}

QTEST_GUILESS_MAIN(TestBarcodeParser)

#include "test_barcodeparser.moc"