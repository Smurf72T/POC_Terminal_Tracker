#ifndef PAYMENTFORM_H
#define PAYMENTFORM_H

#include <QWidget>

namespace Ui {
    class PaymentForm;
}

class PaymentForm : public QWidget
{
    Q_OBJECT

public:
    explicit PaymentForm(QWidget *parent = nullptr);
    ~PaymentForm();

private slots:
    void on_btnSave_clicked();
    void on_btnClose_clicked();

private:
    Ui::PaymentForm *ui;

    void loadClients();
    void loadMonths();
    void loadYears();
    bool checkExistingPayment(int clientId, int month, int year);
};

#endif // PAYMENTFORM_H