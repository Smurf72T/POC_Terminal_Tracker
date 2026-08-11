#ifndef RENTALFORM_H
#define RENTALFORM_H

#include <QDialog>
#include <QStandardItemModel>
#include <QModelIndex>
#include <QMap>

namespace Ui {
    class RentalForm;
}

class RentalForm : public QDialog
{
    Q_OBJECT

public:
    explicit RentalForm(QWidget *parent = nullptr);
    ~RentalForm();

    void loadForEdit(int docId);

private slots:
    void on_btnAddRow_clicked();
    void on_btnDeleteRow_clicked();
    void on_btnPost_clicked();
    void on_btnPrintAct_clicked();  // <-- Добавлено
    void on_btnClose_clicked();
    void onTableViewDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);

private:
    Ui::RentalForm *ui;
    QStandardItemModel *rowsModel;
    bool isPosted = false;
    bool m_editMode = false;
    int m_editDocId = 0;
    // Снимок деталей документа из БД (terminalid -> simcardid) для
    // корректного определения статусов при редактировании проведённого документа.
    QMap<int, int> m_originalDetails;

    void loadClientsToDelegate();
    void loadFreeTerminalsToDelegate();
    void loadFreeSIMsToDelegate();
    void generateDocNumber();
};

#endif // RENTALFORM_H