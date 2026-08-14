#ifndef BARCODEPARSER_H
#define BARCODEPARSER_H

#include <QString>

// Результат разбора «сырой» строки со сканера.
struct BarcodeScan {
    QString serial;
    QString imei1;
    QString imei2;

    bool hasData() const { return !serial.isEmpty() || !imei1.isEmpty() || !imei2.isEmpty(); }
};

// Чистый парсер строк сканера в поля SN/IMEI1/IMEI2 (без GUI, легко тестировать).
class BarcodeParser {
public:
    // Поддерживаемые форматы:
    //  1) Простой SN:                ABC-12345            -> serial
    //  2) Голые 15 цифр:             356938035643809      -> imei1
    //  3) Подписанные поля:          SN:ABC-123 IMEI1:356... IMEI2:356...
    //     разделители — пробел/запятая/табуляция/перевод строки
    //  4) GS1/ASCII-разделители \x1D/\x1E/\x04 нормализуются в пробелы
    //  5) Общий IMEI:… без номера слота -> в первую свободную колонку IMEI
    static BarcodeScan parse(const QString& raw);

    // «Ровно 15 цифр» — эвристика «это IMEI».
    static bool isImei(const QString& value);
};

#endif // BARCODEPARSER_H