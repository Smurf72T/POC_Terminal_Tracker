#ifndef ARCHIVEDOCUMENTSFORM_H
#define ARCHIVEDOCUMENTSFORM_H

#include <QDialog>
#include <QSqlQueryModel>

namespace Ui {
    class ArchiveDocumentsForm;
}

class ArchiveDocumentsForm : public QDialog
{
    Q_OBJECT

public:
    // docType: 1 - Поступление, 2 - Аренда, 3 - Возврат, 4 - Оплата
    explicit ArchiveDocumentsForm(int docType, QWidget *parent = nullptr);
    ~ArchiveDocumentsForm();

private slots:
    void on_btnFilter_clicked();
    void on_btnClose_clicked();

private:
    Ui::ArchiveDocumentsForm *ui;
    QSqlQueryModel *model;
    int m_docType;

    void setupUI();
    void loadClients();
    void applyFilter();
    void setupCheckBoxColumn();
};

#endif // ARCHIVEDOCUMENTSFORM_H