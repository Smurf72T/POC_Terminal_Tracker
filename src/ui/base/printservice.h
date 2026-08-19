#ifndef PRINTSERVICE_H
#define PRINTSERVICE_H

#include <QHash>
#include <QList>
#include <QString>

#include "models/simcard.h"
#include "models/terminal.h"

class QSqlDatabase;
class QWidget;

// Сервис печати документов: печать HTML через QPrinter/QPrintDialog и общие
// HTML-шаблоны, повторяющиеся во всех формах документов.
class PrintService {
public:
    // Печатает HTML через диалог печати. true — если печать выполнена.
    static bool printHtml(const QString& html, QWidget* parent = nullptr);

    // Общая шапка HTML-документа (стили таблиц и базового текста).
    static QString docHeader();
    // Закрывающий тег HTML-документа.
    static QString docFooter();
    // Экранирование HTML-сущностей.
    static QString escapeHtml(const QString& s);

    // Batch-загрузка терминалов/SIM по id для печати (ключ — id записи).
    static QHash<int, models::Terminal> loadTerminalsBatch(const QList<int>& ids, const QSqlDatabase& db);
    static QHash<int, models::SimCard> loadSimsBatch(const QList<int>& ids, const QSqlDatabase& db);
};

#endif // PRINTSERVICE_H
