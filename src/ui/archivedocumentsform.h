#ifndef ARCHIVEDOCUMENTSFORM_H
#define ARCHIVEDOCUMENTSFORM_H

#include <QWidget>
#include <QSqlQueryModel>

namespace Ui {
    class ArchiveDocumentsForm;
}

class ArchiveDocumentsForm : public QWidget
{
    Q_OBJECT

public:
    // docType: 1 - Поступление, 2 - Аренда, 3 - Возврат
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