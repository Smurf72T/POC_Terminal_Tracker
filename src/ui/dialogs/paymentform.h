#ifndef PAYMENTFORM_H
#define PAYMENTFORM_H

#include <QDialog>

namespace Ui {
    class PaymentForm;
}

class PaymentForm : public QDialog
{
    Q_OBJECT

public:
    explicit PaymentForm(QWidget *parent = nullptr);
    ~PaymentForm();

    void loadForEdit(int paymentId);

private slots:
    void on_btnSave_clicked();
    void on_btnPrint_clicked();
    void on_btnClose_clicked();
    void on_comboBoxClient_currentIndexChanged(int index); // <-- Добавлено

private:
    Ui::PaymentForm *ui;

    void loadClients();
    void loadMonths();
    void loadYears();
    void loadRentalDocsForClient(int clientId);
    bool checkExistingPayment(int clientId, int month, int year);

    bool m_editMode = false;
    int m_editPaymentId = 0;
};

#endif // PAYMENTFORM_H