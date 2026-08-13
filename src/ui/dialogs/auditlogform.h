#ifndef AUDITLOGFORM_H
#define AUDITLOGFORM_H

#include <QDialog>
#include <QSqlQueryModel>

namespace Ui {
class AuditLogForm;
}

class AuditLogForm : public QDialog {
    Q_OBJECT

public:
    explicit AuditLogForm(QWidget* parent = nullptr);
    ~AuditLogForm();

private slots:
    void on_btnFilter_clicked();
    void on_btnExportExcel_clicked();
    void on_btnClose_clicked();

private:
    Ui::AuditLogForm* ui;
    QSqlQueryModel* model;

    void loadFilterValues();
    void applyFilter();
};

#endif // AUDITLOGFORM_H
