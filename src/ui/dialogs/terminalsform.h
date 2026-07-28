#ifndef TERMINALSFORM_H
#define TERMINALSFORM_H

#include <QDialog>
#include <QSqlRelationalTableModel>
#include <QTimer>

namespace Ui {
    class TerminalsForm;
}

class TerminalsForm : public QDialog
{
    Q_OBJECT

public:
    explicit TerminalsForm(QWidget *parent = nullptr);
    ~TerminalsForm();

private slots:
    void on_btnAdd_clicked();
    void on_btnDelete_clicked();
    void on_btnClose_clicked();
    void on_model_dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);

private:
    Ui::TerminalsForm *ui;
    QSqlRelationalTableModel *model;
    QTimer *searchTimer;
};

#endif // TERMINALSFORM_H