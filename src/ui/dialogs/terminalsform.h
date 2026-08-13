#ifndef TERMINALSFORM_H
#define TERMINALSFORM_H

#include <QDialog>
#include <QSqlQueryModel>
#include <QTimer>

class QLabel;
class QPushButton;

namespace Ui {
class TerminalsForm;
}

class TerminalsForm : public QDialog {
    Q_OBJECT

public:
    explicit TerminalsForm(QWidget* parent = nullptr);
    ~TerminalsForm();

private slots:
    void on_btnAdd_clicked();
    void on_btnDelete_clicked();
    void on_btnClose_clicked();

private:
    Ui::TerminalsForm* ui;
    QSqlQueryModel* model;
    QTimer* searchTimer;
    void loadModel(const QString& filter = QString());
    void refreshPagination();
    void goToPage(int newOffset);

    int m_pageSize = 1000;
    int m_offset = 0;
    int m_totalRows = 0;
    QLabel* m_pageLabel = nullptr;
    QPushButton* m_btnFirst = nullptr;
    QPushButton* m_btnPrev = nullptr;
    QPushButton* m_btnNext = nullptr;
    QPushButton* m_btnLast = nullptr;
};

#endif // TERMINALSFORM_H
