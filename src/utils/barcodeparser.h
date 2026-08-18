#ifndef BARCODEPARSER_H
#define BARCODEPARSER_H

#include <QString>
#include <QStringList>

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

    // Применяет данные скана к трём параллельным спискам комплектов одной
    // строки документа (серийники/imei1/imei2 — по одному комплекту на индекс).
    // Логика «SN → IMEI 1 → IMEI 2»: серийник открывает новый комплект, голый
    // IMEI дописывается к последнему комплекту (сначала в IMEI 1, затем в
    // IMEI 2), IMEI с подписью «2» — сразу в слот IMEI 2. Возвращает true,
    // если данные изменились.
    static bool applyScan(QStringList& serials, QStringList& imei1, QStringList& imei2, const BarcodeScan& scan);
};

#endif // BARCODEPARSER_H