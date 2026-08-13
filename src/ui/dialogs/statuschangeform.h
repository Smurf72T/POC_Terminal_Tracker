#ifndef STATUSCHANGEFORM_H
#define STATUSCHANGEFORM_H

#include <QDialog>
#include <QStandardItemModel>
#include <QHash>
#include <QSet>

namespace Ui {
class StatusChangeForm;
}

class StatusChangeForm : public QDialog {
    Q_OBJECT

public:
    explicit StatusChangeForm(QWidget* parent = nullptr);
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
    Ui::StatusChangeForm* ui;
    QStandardItemModel* rowsModel;
    bool m_editMode = false;
    int m_editDocId = 0;
    // Снимок состава и прежних статусов терминалов документа для корректного
    // отката статусов при редактировании проведённого документа.
    QSet<int> m_originalTerminals;
    QHash<int, int> m_originalStatus;
    QString m_originalActionType;

    QString actionType() const;
    QString actionTitle() const;
    void updateWindowTitle();
    void loadRepairDocs();
    void loadTerminals();
    void loadTerminalsFromRepairDoc(int repairDocId);
    QList<int> checkedTerminalIds() const;
    bool expectStatus(int status) const;
    int targetStatus() const;
    QString statusText(int status) const;
};

#endif // STATUSCHANGEFORM_H
