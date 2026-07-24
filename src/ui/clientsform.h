#ifndef CLIENTSFORM_H
#define CLIENTSFORM_H

#include <QWidget>
#include <QSqlTableModel>

namespace Ui {
    class ClientsForm;
}

class ClientsForm : public QWidget
{
    Q_OBJECT

public:
    explicit ClientsForm(QWidget *parent = nullptr);
    ~ClientsForm();

private slots:
    void on_btnAdd_clicked();
    void on_btnDelete_clicked();
    void on_btnClose_clicked();

private:
    Ui::ClientsForm *ui;
    QSqlTableModel *model;
};

#endif // CLIENTSFORM_H