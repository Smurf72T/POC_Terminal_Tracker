#include <QTest>
#include <QString>
#include <QSignalSpy>

#include "utils/barcodescanner.h"
#include "utils/barcodeparser.h"

class TestBarcodeScanner : public QObject {
    Q_OBJECT

private slots:
    void feedThenTerminator();
    void feedTooShort();
    void feedCommitByTimeout();
    void swallowedTerminator();
    void ignoresSpacesAndControls();
    void keepsFieldSeparators();
};

void TestBarcodeScanner::feedThenTerminator()
{
    BarcodeScanner scanner;
    QSignalSpy finishedSpy(&scanner, &BarcodeScanner::scanFinished);
    QSignalSpy startedSpy(&scanner, &BarcodeScanner::scanStarted);

    scanner.feed("ABC");
    QVERIFY(scanner.isActive());
    scanner.feed("-12345");
    QVERIFY(finishedSpy.count() == 0);

    QVERIFY(scanner.feedTerminator());
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(finishedSpy.first().at(0).toString(), QString("ABC-12345"));
    QCOMPARE(startedSpy.count(), 1);
    QVERIFY(!scanner.isActive());
}

void TestBarcodeScanner::feedTooShort()
{
    BarcodeScanner scanner;
    QSignalSpy finishedSpy(&scanner, &BarcodeScanner::scanFinished);

    scanner.feed("ab");
    QVERIFY(!scanner.feedTerminator());
    QCOMPARE(finishedSpy.count(), 0);
    QVERIFY(!scanner.isActive());
}

void TestBarcodeScanner::feedCommitByTimeout()
{
    BarcodeScanner scanner;
    scanner.setInterCharTimeoutMs(30);
    QSignalSpy finishedSpy(&scanner, &BarcodeScanner::scanFinished);

    scanner.feed("3569");
    scanner.feed("380");
    QTest::qWait(120); // > timeout: буфер коммитится без терминатора
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(finishedSpy.first().at(0).toString(), QString("3569380"));
}

void TestBarcodeScanner::swallowedTerminator()
{
    BarcodeScanner scanner;
    QSignalSpy finishedSpy(&scanner, &BarcodeScanner::scanFinished);

    QVERIFY(!scanner.feedTerminator()); // буфер пуст — не поглощён
    scanner.feed("XYZ");
    QVERIFY(scanner.feedTerminator());
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(finishedSpy.first().at(0).toString(), QString("XYZ"));
}

void TestBarcodeScanner::ignoresSpacesAndControls()
{
    BarcodeScanner scanner;
    QSignalSpy finishedSpy(&scanner, &BarcodeScanner::scanFinished);

    scanner.feed(" AB C "); // краевые пробелы вырезаются
    QVERIFY(scanner.feedTerminator());
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(finishedSpy.first().at(0).toString(), QString("AB C"));
}

void TestBarcodeScanner::keepsFieldSeparators()
{
    // Подписанный payload с пробелами/GS1-разделителями не «склеивается».
    BarcodeScanner scanner;
    QSignalSpy finishedSpy(&scanner, &BarcodeScanner::scanFinished);

    scanner.feed("SN:ABC-123 IMEI1:356938035643809" + QString(QChar(0x1D)) + "IMEI2:356938035643821");
    QVERIFY(scanner.feedTerminator());
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(finishedSpy.first().at(0).toString(),
             QString("SN:ABC-123 IMEI1:356938035643809 IMEI2:356938035643821"));

    const BarcodeScan data = BarcodeParser::parse(finishedSpy.first().at(0).toString());
    QVERIFY(data.hasData());
    QCOMPARE(data.serial, QString("ABC-123"));
    QCOMPARE(data.imei1, QString("356938035643809"));
    QCOMPARE(data.imei2, QString("356938035643821"));
}

QTEST_GUILESS_MAIN(TestBarcodeScanner)

#include "test_barcodescanner.moc"