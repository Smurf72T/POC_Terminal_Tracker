#ifndef RECEIPTFORM_H
#define RECEIPTFORM_H

#include <QDialog>
#include <QStandardItemModel>

namespace Ui {
    class ReceiptForm;
}

class ReceiptForm : public QDialog
{
    Q_OBJECT

public:
    explicit ReceiptForm(QWidget *parent = nullptr);
    ~ReceiptForm();

private slots:
    void on_btnAddRow_clicked();
    void on_btnDeleteRow_clicked();
    void on_btnPost_clicked(); // Кнопка "Провести"
    void on_btnClose_clicked();

private:
    Ui::ReceiptForm *ui;
    QStandardItemModel *rowsModel;

    void loadModelsToDelegate();
    void generateDocNumber();
};

#endif // RECEIPTFORM_H