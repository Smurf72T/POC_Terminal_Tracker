#ifndef CASEINCOMEFORM_H
#define CASEINCOMEFORM_H

#include "ui/base/documentdialog.h"
#include <QMap>

namespace Ui {
class CaseIncomeForm;
}

// Документ «Поступление чехлов»: строки «тип чехла × количество».
// При проведении создаются свободные чехлы (tblcases status=0).
class CaseIncomeForm : public DocumentDialog {
    Q_OBJECT

public:
    explicit CaseIncomeForm(QWidget* parent = nullptr);
    ~CaseIncomeForm();

private slots:
    void on_btnAddRow_clicked();
    void on_btnDeleteRow_clicked();
    void on_btnPost_clicked();
    void on_btnClose_clicked();

private:
    Ui::CaseIncomeForm* ui;

    // Количества по типам из БД (casetype -> qty) в режиме редактирования.
    QMap<QString, int> m_originalQuantities;

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
};

#endif // CASEINCOMEFORM_H