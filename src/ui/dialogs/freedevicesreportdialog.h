#ifndef FREEDEVICESREPORTDIALOG_H
#define FREEDEVICESREPORTDIALOG_H

#include <QDialog>

class QTableView;

// Отчёт «Свободные терминалы и SIM-карты» с экспортом в Excel.
class FreeDevicesReportDialog : public QDialog {
    Q_OBJECT

public:
    explicit FreeDevicesReportDialog(QWidget* parent = nullptr);

private slots:
    void exportTerminals();
    void exportSimCards();

private:
    QTableView* m_termView = nullptr;
    QTableView* m_simView = nullptr;
};

#endif // FREEDEVICESREPORTDIALOG_H