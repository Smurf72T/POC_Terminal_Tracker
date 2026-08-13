#ifndef TERMINAL_STATUS_H
#define TERMINAL_STATUS_H

#include <QString>

// Словарь статусов терминалов и SIM-карт (tblterminals.status / tblsimcards.status).
//
// CHECK-ограничение расширено миграцией 006_terminal_status_check.sql (0..4).
// Единый источник имён — НЕ дублировать строки «Свободен/В аренде/...» в SQL UI.
namespace TerminalStatus {

enum Value : int {
    Available = 0,  // Свободен
    Rented = 1,     // В аренде
    Repair = 2,     // В ремонте
    WrittenOff = 3, // Списан
    Lost = 4        // Утерян
};

inline constexpr int kMax = 4;

// Название статуса для отображения; для значений вне словаря — «Прочее».
inline QString name(int status)
{
    static const char* const kNames[] = {"Свободен", "В аренде", "В ремонте", "Списан", "Утерян"};
    return (status >= 0 && status <= kMax) ? QString::fromUtf8(kNames[status]) : QStringLiteral("Прочее");
}

// SQL-фрагмент `CASE <col> WHEN 0 THEN ... END` для отображения статуса текстом.
inline QString sqlCaseExpression(const QString& column)
{
    return QStringLiteral("CASE %1 WHEN 0 THEN 'Свободен' WHEN 1 THEN 'В аренде' WHEN 2 THEN 'В ремонте' "
                          "WHEN 3 THEN 'Списан' WHEN 4 THEN 'Утерян' ELSE 'Прочее' END")
        .arg(column);
}

} // namespace TerminalStatus

#endif // TERMINAL_STATUS_H