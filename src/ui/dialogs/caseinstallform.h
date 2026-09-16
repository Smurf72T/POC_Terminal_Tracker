#ifndef CASEINSTALLFORM_H
#define CASEINSTALLFORM_H

#include "ui/base/documentdialog.h"
#include <QMap>
#include <QPair>

namespace Ui {
class CaseInstallForm;
}

// Документ «Установка чехлов»: строки «терминал × чехол».
// Пустая ячейка «Чехол» означает снятие установленного чехла.
class CaseInstallForm : public DocumentDialog {
    Q_OBJECT

public:
    explicit CaseInstallForm(QWidget* parent = nullptr);
    ~CaseInstallForm();

private slots:
    void on_btnAddRow_clicked();
    void on_btnDeleteRow_clicked();
    void on_btnPost_clicked();
    void on_btnClose_clicked();
    void onTableViewDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);

private:
    Ui::CaseInstallForm* ui;

    // Снимок деталей документа из БД (terminalid -> caseid).
    QMap<int, int> m_originalDetails;

    // Уже установленные в свободные терминалы чехлы (terminalId -> {caseId, тип}).
    QMap<int, QPair<int, QString>> m_installedCase;

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

    void loadFreeTerminalsToDelegate();
    void loadCasesToDelegate();
    // Автозаполнение чехла при выборе терминала в колонке 0.
    void autoFillCaseForTerminal(int row, int terminalId);
};

#endif // CASEINSTALLFORM_H