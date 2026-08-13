#ifndef EXPIRYNOTIFICATIONSFORM_H
#define EXPIRYNOTIFICATIONSFORM_H

#include <QDialog>
#include <QSqlQueryModel>

namespace Ui {
class ExpiryNotificationsForm;
}

class ExpiryNotificationsForm : public QDialog {
    Q_OBJECT

public:
    explicit ExpiryNotificationsForm(QWidget* parent = nullptr);
    ~ExpiryNotificationsForm();

private slots:
    void on_btnRefresh_clicked();
    void on_btnExport_clicked();
    void on_btnClose_clicked();

private:
    Ui::ExpiryNotificationsForm* ui;
    QSqlQueryModel* overdueModel;
    QSqlQueryModel* unpaidModel;

    void loadOverdueRentals();
    void loadUnpaidPeriods();
};

#endif // EXPIRYNOTIFICATIONSFORM_H
