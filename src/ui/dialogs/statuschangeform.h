#ifndef STATUSCHANGEFORM_H
#define STATUSCHANGEFORM_H

#include "ui/base/documentdialog.h"
#include <QHash>
#include <QSet>

namespace Ui {
class StatusChangeForm;
}

class QSqlDatabase;

class StatusChangeForm : public DocumentDialog {
    Q_OBJECT

public:
    explicit StatusChangeForm(QWidget* parent = nullptr);
    ~StatusChangeForm();

private slots:
    void on_comboBoxActionType_currentIndexChanged(int index);
    void on_comboBoxRepairDoc_currentIndexChanged(int index);
    void on_btnRefresh_clicked();
    void on_btnPost_clicked();
    void on_btnPrint_clicked();
    void on_btnClose_clicked();

private:
    Ui::StatusChangeForm* ui;

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

    // Заполняется в validateBeforePost и используется в postHeader/postDetails.
    QString m_comment;
    QList<int> m_terminalIds;

    // Снимок состава и прежних статусов терминалов документа для корректного
    // отката статусов при редактировании проведённого документа.
    QSet<int> m_originalTerminals;
    QHash<int, int> m_originalStatus;
    QString m_originalActionType;

    QString actionType() const;
    void updateWindowTitle();
    void loadRepairDocs();
    void loadTerminals();
    void loadTerminalsFromRepairDoc(int repairDocId);
    QList<int> checkedTerminalIds() const;
};

#endif // STATUSCHANGEFORM_H