#ifndef BATCHSTATUSFORM_H
#define BATCHSTATUSFORM_H

#include <QDialog>
#include <QSqlQueryModel>

namespace Ui {
class BatchStatusForm;
}

class BatchStatusForm : public QDialog {
    Q_OBJECT

public:
    explicit BatchStatusForm(QWidget* parent = nullptr);
    ~BatchStatusForm();

private slots:
    void on_btnApply_clicked();
    void on_btnSelectAll_clicked();
    void on_btnDeselectAll_clicked();
    void on_btnClose_clicked();
    void on_comboBoxCurrentStatus_currentIndexChanged(int index);

private:
    Ui::BatchStatusForm* ui;
    QSqlQueryModel* model;

    void loadStatuses();
    void loadTerminals(int currentStatus);
};

#endif // BATCHSTATUSFORM_H
