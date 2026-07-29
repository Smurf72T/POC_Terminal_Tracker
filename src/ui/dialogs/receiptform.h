#ifndef RECEIPTFORM_H
#define RECEIPTFORM_H

#include <QDialog>
#include <QStandardItemModel>
#include <QPair>

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

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    Ui::ReceiptForm *ui;
    QStandardItemModel *rowsModel;

    void loadModelsToDelegate();
    void generateDocNumber();

    QList<QPair<int, QString>> m_models;
};

#endif // RECEIPTFORM_H