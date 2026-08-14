#include "utils/barcodeparser.h"

#include <QRegularExpression>
#include <QList>

namespace {

// Убирает не-буквенно-цифровые символы по краям значения (кавычки, скобки и т.п.).
QString cleanValue(const QString& value)
{
    QString v = value.trimmed();
    int start = 0;
    int end = v.size();
    while (start < end && !v[start].isLetterOrNumber())
        ++start;
    while (end > start && !v[end - 1].isLetterOrNumber())
        --end;
    return v.mid(start, end - start);
}

// Позиция ключа-подписи в строке сканера.
struct KeyMatch {
    qsizetype start = 0;
    qsizetype end = 0;
    QString text; // нормализовано: "SN", "IMEI", "IMEI1", "IMEI2"
};

// Ищет все подписи SN/IMEI[1|2] на границах слов. "IMEI" внутри "IMEI1"
// не дублируется: альтернативы перебираются от длинной к короткой.
QList<KeyMatch> collectKeys(const QString& text)
{
    QList<KeyMatch> out;
    static const QRegularExpression keyRe("(?<![A-Za-z0-9])(IMEI\\s*[12]|IMEI|SN)(?![A-Za-z0-9])",
                                          QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator it = keyRe.globalMatch(text);
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        QString norm = m.captured().toUpper().remove(' ');
        out.append({m.capturedStart(), m.capturedEnd(), norm});
    }
    return out;
}

} // namespace

bool BarcodeParser::isImei(const QString& value)
{
    if (value.size() != 15)
        return false;
    for (QChar ch : value) {
        if (!ch.isDigit())
            return false;
    }
    return true;
}

BarcodeScan BarcodeParser::parse(const QString& raw)
{
    BarcodeScan out;
    QString text = raw.trimmed();
    if (text.isEmpty())
        return out;

    // GS1 / ASCII-разделители -> пробел.
    text.replace(QChar(0x1D), ' ').replace(QChar(0x1E), ' ').replace(QChar(0x04), ' ');

    const QList<KeyMatch> keys = collectKeys(text);

    // Без подписей: либо голый IMEI (15 цифр), либо простой SN.
    if (keys.isEmpty()) {
        const QString value = cleanValue(text);
        if (isImei(value))
            out.imei1 = value;
        else if (value.length() >= 3)
            out.serial = value;
        return out;
    }

    // Подписанные поля: значение ключа — текст до следующего ключа.
    for (int i = 0; i < keys.size(); ++i) {
        const KeyMatch& key = keys[i];
        int valueStart = key.end;
        // Пропускаем пробелы и разделитель ":" / "=".
        while (valueStart < text.size() && text[valueStart].isSpace())
            ++valueStart;
        if (valueStart < text.size() && (text[valueStart] == ':' || text[valueStart] == '='))
            ++valueStart;
        while (valueStart < text.size() && text[valueStart].isSpace())
            ++valueStart;

        int valueEnd = text.size();
        if (i + 1 < keys.size())
            valueEnd = keys[i + 1].start;

        const QString value = cleanValue(text.mid(valueStart, valueEnd - valueStart));
        if (value.isEmpty())
            continue;

        if (key.text == "SN") {
            if (out.serial.isEmpty())
                out.serial = value;
        } else if (key.text == "IMEI" || key.text == "IMEI1") {
            if (out.imei1.isEmpty())
                out.imei1 = value;
            else if (out.imei2.isEmpty())
                out.imei2 = value;
        } else if (key.text == "IMEI2") {
            if (out.imei2.isEmpty())
                out.imei2 = value;
            else if (out.imei1.isEmpty())
                out.imei1 = value;
        }
    }

    return out;
}