#include "reportexporter.h"
#include <xlsxdocument.h>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextDocument>
#include <QTextCursor>
#include <QPrinter>
#include <QDateTime>
#include <QFileInfo>
#include <QDebug>

ReportExporter::ReportExporter(QObject* parent) : QObject(parent) {}

QString ReportExporter::getSaveFilePath(QWidget* parent, const QString& title, const QString& filter)
{
    return QFileDialog::getSaveFileName(parent, title, QDir::homePath(), filter);
}

bool ReportExporter::exportModelToExcel(QSqlQueryModel* model, const QString& title, const QString& filePath,
                                        QWidget* parent)
{
    if (!model || model->rowCount() == 0) {
        // Модальное окно с parent==nullptr заблокирует процесс в headless-среде
        // (CI): показываем предупреждение только когда есть родитель (UI-вызов).
        if (parent)
            QMessageBox::warning(parent, "Экспорт", "Нет данных для экспорта!");
        return false;
    }

    QXlsx::Document xlsx;

    // Заголовок
    QXlsx::Format headerFormat;
    headerFormat.setFontBold(true);
    headerFormat.setFontSize(12);
    headerFormat.setFillPattern(QXlsx::Format::PatternSolid);
    headerFormat.setPatternBackgroundColor(QColor("#4472C4"));
    headerFormat.setFontColor(Qt::white);
    headerFormat.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
    headerFormat.setBorderStyle(QXlsx::Format::BorderThin);

    QXlsx::Format cellFormat;
    cellFormat.setBorderStyle(QXlsx::Format::BorderThin);

    // Заголовок отчёта
    xlsx.write(1, 1, title);
    xlsx.write(2, 1, "Дата формирования: " + QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss"));

    // Шапка таблицы (строка 4)
    int headerRow = 4;
    for (int col = 0; col < model->columnCount(); ++col) {
        QString headerText = model->headerData(col, Qt::Horizontal).toString();
        xlsx.write(headerRow, col + 1, headerText, headerFormat);
    }

    // Данные
    for (int row = 0; row < model->rowCount(); ++row) {
        for (int col = 0; col < model->columnCount(); ++col) {
            QVariant value = model->data(model->index(row, col));
            xlsx.write(headerRow + 1 + row, col + 1, value, cellFormat);
        }
    }

    // Автоширина колонок
    for (int col = 1; col <= model->columnCount(); ++col) {
        xlsx.setColumnWidth(col, 20);
    }

    return xlsx.saveAs(filePath);
}

bool ReportExporter::exportHtmlToPdf(const QString& html, const QString& filePath, QWidget* parent)
{
    QTextDocument doc;
    doc.setHtml(html);

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(filePath);
    printer.setPageMargins(QMarginsF(15, 15, 15, 15));

    doc.print(&printer);

    return QFile::exists(filePath) && QFileInfo(filePath).size() > 0;
}