#ifndef RETURNFORM_H
#define RETURNFORM_H

#include <QWidget>
#include <QStandardItemModel>

namespace Ui {
    class ReturnForm;
}

class ReturnForm : public QWidget
{
    Q_OBJECT

public:
    explicit ReturnForm(QWidget *parent = nullptr);
    ~ReturnForm();

private slots:
    void on_comboBoxClient_currentIndexChanged(int index);
    void on_comboBoxRentalDoc_currentIndexChanged(int index);
    void on_btnPost_clicked();
    void on_btnClose_clicked();

private:
    Ui::ReturnForm *ui;
    QStandardItemModel *rowsModel;

    void loadClientsToComboBox();
    void loadRentalDocs(int clientId);
    void loadRentalDetails(int rentalDocId);
    void generateDocNumber();
};

#endif // RETURNFORM_H