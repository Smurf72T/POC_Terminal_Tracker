#ifndef MANUFACTURERSFORM_H
#define MANUFACTURERSFORM_H

#include <QWidget>
#include <QSqlTableModel>
#include <QCloseEvent>

namespace Ui {
    class ManufacturersForm;
}

class ManufacturersForm : public QWidget
{
    Q_OBJECT

public:
    explicit ManufacturersForm(QWidget *parent = nullptr);
    ~ManufacturersForm();

private slots:
    void on_btnAdd_clicked();
    void on_btnDelete_clicked();
    void on_btnClose_clicked();

private:
    Ui::ManufacturersForm *ui;
    QSqlTableModel *model;
};

#endif // MANUFACTURERSFORM_H