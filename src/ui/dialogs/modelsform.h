#ifndef MODELSFORM_H
#define MODELSFORM_H

#include <QDialog>
#include <QSqlRelationalTableModel>
#include <QCloseEvent>
#include <QTimer>

namespace Ui {
    class ModelsForm;
}

class ModelsForm : public QDialog
{
    Q_OBJECT

public:
    explicit ModelsForm(QWidget *parent = nullptr);
    ~ModelsForm();

private slots:
    void on_btnAdd_clicked();
    void on_btnDelete_clicked();
    void on_btnClose_clicked();

private:
    Ui::ModelsForm *ui;
    QSqlRelationalTableModel *model;
    QTimer *searchTimer;
};

#endif // MODELSFORM_H