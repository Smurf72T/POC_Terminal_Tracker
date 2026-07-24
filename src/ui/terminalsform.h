#ifndef TERMINALSFORM_H
#define TERMINALSFORM_H

#include <QWidget>
#include <QSqlRelationalTableModel>

namespace Ui {
    class TerminalsForm;
}

class TerminalsForm : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalsForm(QWidget *parent = nullptr);
    ~TerminalsForm();

private slots:
    void on_btnAdd_clicked();
    void on_btnDelete_clicked();
    void on_btnClose_clicked();

private:
    Ui::TerminalsForm *ui;
    QSqlRelationalTableModel *model;
};

#endif // TERMINALSFORM_H