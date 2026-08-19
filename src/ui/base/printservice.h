#ifndef PRINTSERVICE_H
#define PRINTSERVICE_H

#include <QString>

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
};

#endif // PRINTSERVICE_H
