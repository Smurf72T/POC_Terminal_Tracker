#ifndef TERMINALHISTORYFORM_H
#define TERMINALHISTORYFORM_H

#include <QDialog>
#include <QSqlQueryModel>

namespace Ui {
    class TerminalHistoryForm;
}

class TerminalHistoryForm : public QDialog
{
    Q_OBJECT

public:
    explicit TerminalHistoryForm(int terminalId, QString serialNumber, QWidget *parent = nullptr);
    ~TerminalHistoryForm();

private slots:
    void on_btnExportExcel_clicked();
    void on_btnClose_clicked();
    void on_tabWidget_tabBarClicked(int index);

private:
    Ui::TerminalHistoryForm *ui;
    QSqlQueryModel *receiptModel;
    QSqlQueryModel *rentalModel;
    QSqlQueryModel *returnModel;
    QSqlQueryModel *paymentModel;

    void loadHistory(int terminalId, const QString &serialNumber);
    QString getSaveFilePath(const QString &title, const QString &filter);
};

#endif // TERMINALHISTORYFORM_H
