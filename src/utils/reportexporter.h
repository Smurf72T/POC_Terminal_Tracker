#ifndef REPORTEXPORTER_H
#define REPORTEXPORTER_H

#include <QObject>
#include <QString>
#include <QSqlQueryModel>
#include <QWidget>

class ReportExporter : public QObject {
    Q_OBJECT
public:
    explicit ReportExporter(QObject* parent = nullptr);

    // Экспорт модели данных в Excel
    static bool exportModelToExcel(QSqlQueryModel* model, const QString& title, const QString& filePath,
                                   QWidget* parent = nullptr);

    // Экспорт произвольного HTML в PDF
    static bool exportHtmlToPdf(const QString& html, const QString& filePath, QWidget* parent = nullptr);

    // Диалог выбора файла
    static QString getSaveFilePath(QWidget* parent, const QString& title, const QString& filter);
};

#endif // REPORTEXPORTER_H