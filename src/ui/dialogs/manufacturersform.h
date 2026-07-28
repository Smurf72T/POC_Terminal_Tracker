#ifndef MANUFACTURERSFORM_H
#define MANUFACTURERSFORM_H

#include <QDialog>
#include <QSqlTableModel>
#include <QCloseEvent>
#include <QTimer>

namespace Ui {
    class ManufacturersForm;
}

class ManufacturersForm : public QDialog
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
    QTimer *searchTimer;
};

#endif // MANUFACTURERSFORM_H