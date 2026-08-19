#ifndef RETURNFORM_H
#define RETURNFORM_H

#include "ui/base/clientdocdialog.h"
#include <QSet>

namespace Ui {
class ReturnForm;
}

class QSqlDatabase;

class ReturnForm : public ClientDocumentDialog {
    Q_OBJECT

public:
    explicit ReturnForm(QWidget* parent = nullptr);
    ~ReturnForm();

private slots:
    void on_comboBoxClient_currentIndexChanged(int index);
    void on_comboBoxRentalDoc_currentIndexChanged(int index);
    void on_btnPost_clicked();
    void on_btnPrint_clicked();
    void on_btnClose_clicked();

private:
    Ui::ReturnForm* ui;

    void loadRentalDetails(int rentalDocId);

    // --- DocumentDialog ---
    QString docType() const override;
    QLineEdit* headerNumberEdit() const override;
    QDateEdit* headerDateEdit() const override;
    QTextEdit* headerCommentEdit() const override;
    QTableView* tableView() const override;
    bool validateBeforePost() override;
    int postHeader(QSqlDatabase& db) override;
    bool postDetails(QSqlDatabase& db, int docId) override;
    void onPostSuccess(int docId) override;
    void loadSpecificEditData(int docId) override;

    // Терминалы, отмеченные для возврата (заполняется в validateBeforePost).
    QList<int> m_terminalsToReturn;
    // Cнимок возвращённых терминалов и связанного документа аренды для
    // корректной обработки статусов при редактировании проведённого возврата.
    QSet<int> m_originalReturned;
    int m_editRentalDocId = 0;
};

#endif // RETURNFORM_H