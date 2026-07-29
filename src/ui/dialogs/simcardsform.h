#ifndef SIMCARDSFORM_H
#define SIMCARDSFORM_H

#include <QDialog>
#include <QCloseEvent>
#include <QSqlTableModel>
#include <QTimer>
#include "delegates/readonlydelegate.h"

namespace Ui {
    class SIMCardsForm;
}

class SIMCardsForm : public QDialog
{
    Q_OBJECT

public:
    explicit SIMCardsForm(QWidget *parent = nullptr);
    ~SIMCardsForm();

private slots:
    void on_btnAdd_clicked();
    void on_btnDelete_clicked();
    void on_btnClose_clicked();

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    Ui::SIMCardsForm *ui;
    QSqlTableModel *model;
    QTimer *searchTimer;
};

#endif // SIMCARDSFORM_H