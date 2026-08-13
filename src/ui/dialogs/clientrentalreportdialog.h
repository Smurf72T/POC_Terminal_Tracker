#ifndef CLIENTRENTALREPORTDIALOG_H
#define CLIENTRENTALREPORTDIALOG_H

#include <QDialog>
#include <QString>

class QSqlQueryModel;

// Отчёт «Клиент — Терминалы в аренде» с экспортом в Excel.
class ClientRentalReportDialog : public QDialog {
    Q_OBJECT

public:
    explicit ClientRentalReportDialog(int clientId, const QString& clientName, QWidget* parent = nullptr);

private slots:
    void exportReport();

private:
    QSqlQueryModel* m_model = nullptr;
    QString m_clientName;
};

#endif // CLIENTRENTALREPORTDIALOG_H