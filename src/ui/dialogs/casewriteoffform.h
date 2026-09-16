#ifndef CASEWRITEOFFFORM_H
#define CASEWRITEOFFFORM_H

#include "ui/base/documentdialog.h"
#include <QSet>

namespace Ui {
class CaseWriteoffForm;
}

// Документ «Списание чехлов»: выбираются чехлы (на складе или установленные),
// для каждого указывается причина. Отмеченные чехлы переводятся в статус 2.
class CaseWriteoffForm : public DocumentDialog {
    Q_OBJECT

public:
    explicit CaseWriteoffForm(QWidget* parent = nullptr);
    ~CaseWriteoffForm();

private slots:
    void on_btnAddRow_clicked();
    void on_btnDeleteRow_clicked();
    void on_btnPost_clicked();
    void on_btnClose_clicked();

private:
    Ui::CaseWriteoffForm* ui;

    // Множество списанных caseId из документа (режим редактирования).
    QSet<int> m_originalCaseIds;

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

    void reloadCasesToTable();
};

#endif // CASEWRITEOFFFORM_H