#ifndef CASESFORM_H
#define CASESFORM_H

#include <QDialog>
#include <QCloseEvent>
#include <QEvent>
#include <QKeyEvent>
#include <QSqlRelationalTableModel>
#include <QTimer>
#include "delegates/readonlydelegate.h"

namespace Ui {
class CasesForm;
}

class CasesForm : public QDialog {
    Q_OBJECT

public:
    explicit CasesForm(QWidget* parent = nullptr);
    ~CasesForm();

private slots:
    void on_btnAdd_clicked();
    void on_btnDelete_clicked();
    void on_btnClose_clicked();

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    Ui::CasesForm* ui;
    QSqlRelationalTableModel* model;
    QTimer* searchTimer;
};

#endif // CASESFORM_H