#ifndef RENTALFORM_H
#define RENTALFORM_H

#include "ui/base/clientdocdialog.h"
#include <QMap>
#include <QPair>

namespace Ui {
class RentalForm;
}

class QSqlDatabase; // forward declaration (методы принимают ссылку)

class RentalForm : public ClientDocumentDialog {
    Q_OBJECT

public:
    explicit RentalForm(QWidget* parent = nullptr);
    ~RentalForm();

private slots:
    void on_btnAddRow_clicked();
    void on_btnDeleteRow_clicked();
    void on_btnPost_clicked();
    void on_btnPrintAct_clicked();
    void on_btnClose_clicked();
    void onTableViewDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);

private:
    Ui::RentalForm* ui;
    bool isPosted = false;
    // Снимок деталей документа из БД (terminalid -> {sim слота 1, sim слота 2})
    // для корректного определения статусов при редактировании проведённого документа.
    QMap<int, QPair<int, int>> m_originalDetails;

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
    void loadFreeSIMsToDelegate();
};

#endif // RENTALFORM_H