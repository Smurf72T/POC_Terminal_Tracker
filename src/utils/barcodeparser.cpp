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

bool BarcodeParser::applyScan(QStringList& serials, QStringList& imei1, QStringList& imei2, const BarcodeScan& scan)
{
    bool changed = false;

    // Серийный номер: новый комплект. Если последняя строка осталась без
    // серийника (например, первым отсканировали IMEI) — дописываем в неё.
    if (!scan.serial.isEmpty()) {
        const int last = serials.size() - 1;
        if (last >= 0 && serials.at(last).isEmpty()) {
            serials[last] = scan.serial;
        } else {
            serials.append(scan.serial);
            imei1.append(QString());
            imei2.append(QString());
        }
        changed = true;
    }

    // IMEI дописывается к «текущему» комплекту — последнему, у которого есть
    // серийный номер (SN → IMEI 1 → IMEI 2 → следующий SN). Если серийник ещё
    // не отсканирован, «текущим» считается последняя строка вообще. Если
    // текущий комплект заполнен полностью или его нет — начинаем новый без SN.
    auto fillImei = [&](const QString& value, bool preferSlot2) {
        if (value.isEmpty())
            return;
        int idx = -1;
        for (int k = serials.size() - 1; k >= 0; --k) {
            if (!serials.at(k).isEmpty()) {
                idx = k;
                break;
            }
        }
        if (idx < 0 && !serials.isEmpty())
            idx = serials.size() - 1;

        if (idx >= 0) {
            const bool slot1Empty = imei1.at(idx).isEmpty();
            const bool slot2Empty = imei2.at(idx).isEmpty();
            if (preferSlot2) {
                if (slot2Empty) {
                    imei2[idx] = value;
                    changed = true;
                    return;
                }
                if (slot1Empty) {
                    imei1[idx] = value;
                    changed = true;
                    return;
                }
            } else {
                if (slot1Empty) {
                    imei1[idx] = value;
                    changed = true;
                    return;
                }
                if (slot2Empty) {
                    imei2[idx] = value;
                    changed = true;
                    return;
                }
            }
        }
        serials.append(QString());
        imei1.append(preferSlot2 ? QString() : value);
        imei2.append(preferSlot2 ? value : QString());
        changed = true;
    };

    fillImei(scan.imei1, false);
    fillImei(scan.imei2, true);
    return changed;
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