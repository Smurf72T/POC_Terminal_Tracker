#include "ui/base/printservice.h"
#include "database/repositories/simcardrepository.h"
#include "database/repositories/terminalrepository.h"
#include <QPrinter>
#include <QPrintDialog>
#include <QTextDocument>
#include <QWidget>

bool PrintService::printHtml(const QString& html, QWidget* parent)
{
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog printDialog(&printer, parent);
    if (printDialog.exec() != QDialog::Accepted)
        return false;

    QTextDocument doc;
    doc.setHtml(html);
    doc.print(&printer);
    return true;
}

QString PrintService::docHeader()
{
    return "<html><head><meta charset='utf-8'>"
           "<style>"
           "body { font-family: 'Times New Roman', serif; font-size: 14px; }"
           "h2 { text-align: center; }"
           "table { border-collapse: collapse; width: 100%; margin-top: 20px; }"
           "th, td { border: 1px solid black; padding: 6px; text-align: left; vertical-align: top; }"
           "th { background-color: #f0f0f0; }"
           "</style></head><body>";
}

QString PrintService::docFooter()
{
    return "</body></html>";
}

QString PrintService::escapeHtml(const QString& s)
{
    return s.toHtmlEscaped();
}

QHash<int, models::Terminal> PrintService::loadTerminalsBatch(const QList<int>& ids, const QSqlDatabase& db)
{
    QHash<int, models::Terminal> result;
    for (const auto& t : TerminalRepository(db).loadByIds(ids))
        result.insert(t.id, t);
    return result;
}

QHash<int, models::SimCard> PrintService::loadSimsBatch(const QList<int>& ids, const QSqlDatabase& db)
{
    QHash<int, models::SimCard> result;
    for (const auto& s : SimCardRepository(db).loadByIds(ids))
        result.insert(s.id, s);
    return result;
}
