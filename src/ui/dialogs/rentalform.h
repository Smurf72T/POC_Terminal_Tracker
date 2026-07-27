#ifndef RENTALFORM_H
#define RENTALFORM_H

#include <QDialog>
#include <QStandardItemModel>
#include <QModelIndex>

namespace Ui {
    class RentalForm;
}

class RentalForm : public QDialog
{
    Q_OBJECT

public:
    explicit RentalForm(QWidget *parent = nullptr);
    ~RentalForm();

private slots:
    void on_btnAddRow_clicked();
    void on_btnDeleteRow_clicked();
    void on_btnPost_clicked();
    void on_btnPrintAct_clicked();  // <-- Добавлено
    void on_btnClose_clicked();
    void onTableViewDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);

private:
    Ui::RentalForm *ui;
    QStandardItemModel *rowsModel;

    void loadClientsToDelegate();
    void loadFreeTerminalsToDelegate();
    void loadFreeSIMsToDelegate();
    void generateDocNumber();
    void ensureCommentColumn();
};

#endif // RENTALFORM_H