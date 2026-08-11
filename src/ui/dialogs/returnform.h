#ifndef RETURNFORM_H
#define RETURNFORM_H

#include <QDialog>
#include <QStandardItemModel>
#include <QSet>

namespace Ui {
    class ReturnForm;
}

class ReturnForm : public QDialog
{
    Q_OBJECT

public:
    explicit ReturnForm(QWidget *parent = nullptr);
    ~ReturnForm();

    void loadForEdit(int docId);

private slots:
    void on_comboBoxClient_currentIndexChanged(int index);
    void on_comboBoxRentalDoc_currentIndexChanged(int index);
    void on_btnPost_clicked();
    void on_btnPrint_clicked();
    void on_btnClose_clicked();

private:
    Ui::ReturnForm *ui;
    QStandardItemModel *rowsModel;

    void loadClientsToComboBox();
    void loadRentalDocs(int clientId);
    void loadRentalDetails(int rentalDocId);

    bool m_editMode = false;
    int m_editDocId = 0;
    // Cнимок возвращённых терминалов и связанного документа аренды для
    // корректной обработки статусов при редактировании проведённого возврата.
    QSet<int> m_originalReturned;
    int m_editRentalDocId = 0;
};

#endif // RETURNFORM_H