#ifndef MODELSFORM_H
#define MODELSFORM_H

#include <QWidget>
#include <QSqlRelationalTableModel>
#include <QCloseEvent>

namespace Ui {
    class ModelsForm;
}

class ModelsForm : public QWidget
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
};

#endif // MODELSFORM_H