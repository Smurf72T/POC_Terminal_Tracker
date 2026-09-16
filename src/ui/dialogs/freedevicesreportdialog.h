#ifndef FREEDEVICESREPORTDIALOG_H
#define FREEDEVICESREPORTDIALOG_H

#include <QDialog>

class QTableView;

// Отчёт «Свободные терминалы, SIM-карты и чехлы» с экспортом в Excel.
class FreeDevicesReportDialog : public QDialog {
    Q_OBJECT

public:
    explicit FreeDevicesReportDialog(QWidget* parent = nullptr);

private slots:
    void exportTerminals();
    void exportSimCards();
    void exportCases();

private:
    QTableView* m_termView = nullptr;
    QTableView* m_simView = nullptr;
    QTableView* m_caseView = nullptr;
};

#endif // FREEDEVICESREPORTDIALOG_H