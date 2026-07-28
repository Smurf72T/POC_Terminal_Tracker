#ifndef CLIENTSFORM_H
#define CLIENTSFORM_H

#include <QDialog>
#include <QSqlTableModel>
#include <QTimer>

namespace Ui {
    class ClientsForm;
}

class ClientsForm : public QDialog
{
    Q_OBJECT

public:
    explicit ClientsForm(QWidget *parent = nullptr);
    ~ClientsForm();

private slots:
    void on_btnAdd_clicked();
    void on_btnDelete_clicked();
    void on_btnClose_clicked();
    void on_lineEditSearch_textChanged(const QString &arg1);

private:
    Ui::ClientsForm *ui;
    QSqlTableModel *model;
    QTimer *searchTimer;
};

#endif // CLIENTSFORM_H