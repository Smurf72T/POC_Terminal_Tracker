#include "ui/base/printservice.h"
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
