#ifndef BULKIMPORTFORM_H
#define BULKIMPORTFORM_H

#include <QDialog>
#include <QSqlQueryModel>

namespace Ui {
    class BulkImportForm;
}

class BulkImportForm : public QDialog
{
    Q_OBJECT

public:
    explicit BulkImportForm(QWidget *parent = nullptr);
    ~BulkImportForm();

private slots:
    void on_btnSelectFile_clicked();
    void on_btnImport_clicked();
    void on_btnClose_clicked();
    void on_btnExportTemplate_clicked();

private:
    Ui::BulkImportForm *ui;
    QString selectedFilePath;
    QSqlQueryModel *previewModel;

    void loadPreview();
    bool importData();
    void showImportResult(bool success, int totalRows, int importedRows, int failedRows, const QString &errorMsg);
};

#endif // BULKIMPORTFORM_H
