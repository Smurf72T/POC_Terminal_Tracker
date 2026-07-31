#ifndef STATUSCHANGEFORM_H
#define STATUSCHANGEFORM_H

#include <QDialog>
#include <QStandardItemModel>

namespace Ui {
    class StatusChangeForm;
}

class StatusChangeForm : public QDialog
{
    Q_OBJECT

public:
    explicit StatusChangeForm(QWidget *parent = nullptr);
    ~StatusChangeForm();

    void loadForEdit(int docId);

private slots:
    void on_comboBoxActionType_currentIndexChanged(int index);
    void on_comboBoxRepairDoc_currentIndexChanged(int index);
    void on_btnRefresh_clicked();
    void on_btnPost_clicked();
    void on_btnPrint_clicked();
    void on_btnClose_clicked();

private:
    Ui::StatusChangeForm *ui;
    QStandardItemModel *rowsModel;
    bool m_editMode = false;
    int m_editDocId = 0;

    QString actionType() const;
    QString actionTitle() const;
    void updateWindowTitle();
    void generateDocNumber();
    void loadRepairDocs();
    void loadTerminals();
    void loadTerminalsFromRepairDoc(int repairDocId);
    QList<int> checkedTerminalIds() const;
    bool expectStatus(int status) const;
    int targetStatus() const;
    QString statusText(int status) const;
};

#endif // STATUSCHANGEFORM_H
