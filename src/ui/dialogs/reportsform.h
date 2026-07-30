#ifndef REPORTSFORM_H
#define REPORTSFORM_H

#include <QDialog>
#include <QSqlQueryModel>

namespace Ui {
    class ReportsForm;
}

class ReportsForm : public QDialog
{
    Q_OBJECT

public:
    explicit ReportsForm(QWidget *parent = nullptr);
    ~ReportsForm();

private slots:
    void on_btnGenerate_clicked();
    void on_btnExport_clicked();
    void on_btnClose_clicked();

private:
    Ui::ReportsForm *ui;
    QSqlQueryModel *model;

    void loadReportTypes();
    void generateRevenueByClient();
    void generateTerminalLoad();
    void generateRentalConversion();
    void generateSIMUsage();
    void generateDebtReport();
    void generateSummary(const QString &text);
};

#endif // REPORTSFORM_H
