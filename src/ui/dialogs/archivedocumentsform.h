#ifndef ARCHIVEDOCUMENTSFORM_H
#define ARCHIVEDOCUMENTSFORM_H

#include <QDialog>
#include <QSqlQueryModel>
#include <QDate>

namespace Ui {
class ArchiveDocumentsForm;
}

class ArchiveDocumentsForm : public QDialog {
    Q_OBJECT

public:
    // docType: 1 - Поступление, 2 - Аренда, 3 - Возврат, 4 - Оплата, 5 - Изменение статусов
    explicit ArchiveDocumentsForm(int docType, QWidget* parent = nullptr);
    ~ArchiveDocumentsForm();

private slots:
    void on_btnFilter_clicked();
    void on_btnExportExcel_clicked();
    void on_btnExportPdf_clicked();
    void on_btnClose_clicked();
    void on_tableView_doubleClicked(const QModelIndex& index);

private:
    Ui::ArchiveDocumentsForm* ui;
    QSqlQueryModel* model;
    int m_docType;
    // Исходные даты фильтра (при открытии). Пока даты не изменялись —
    // датами не фильтруем и показываем все документы.
    QDate m_initialFrom;
    QDate m_initialTo;

    void setupUI();
    void loadClients();
    void applyFilter();
    void setupCheckBoxColumn();
    int getDocIdFromRow(int row) const;
    void openReceiptForEdit(int docId);
    void openRentalForEdit(int docId);
    void openReturnForEdit(int docId);
    void openPaymentForEdit(int docId);
    void openStatusChangeForEdit(int docId);
};

#endif // ARCHIVEDOCUMENTSFORM_H