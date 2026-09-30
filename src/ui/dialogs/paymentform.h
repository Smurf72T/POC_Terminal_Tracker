#ifndef PAYMENTFORM_H
#define PAYMENTFORM_H

#include "ui/base/clientdocdialog.h"

#include <QList>
#include <QSet>

class QStandardItemModel;

namespace Ui {
class PaymentForm;
}

class QSqlDatabase;

class PaymentForm : public ClientDocumentDialog {
    Q_OBJECT

public:
    explicit PaymentForm(QWidget* parent = nullptr);
    ~PaymentForm();

private slots:
    void on_btnSave_clicked();
    void on_btnPrint_clicked();
    void on_btnClose_clicked();
    void on_comboBoxClient_currentIndexChanged(int index); // <-- Добавлено

private:
    Ui::PaymentForm* ui;

    // Колонки таблицы привязки оплаты к документам аренды.
    enum LinkColumn { ColRental = 0, ColPaid = 1 };
    // Роль данных, в которой у элемента колонки ColRental лежит rentaldocid.
    static constexpr int kRentalIdRole = Qt::UserRole;

    void loadMonths();
    void loadYears();
    void loadRentalDocsForClient(int clientId);
    void applyLinkedRentalDocs(const QSet<int>& linkedRentalIds);

    // --- DocumentDialog ---
    // У оплаты нет табличной части и поля номера, поэтому tableView() и
    // headerNumberEdit() возвращают nullptr (базовый класс их не использует).
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

    // Выбранные документы аренды (заполняется в validateBeforePost).
    QList<int> m_selectedRentalIds;
};

#endif // PAYMENTFORM_H
