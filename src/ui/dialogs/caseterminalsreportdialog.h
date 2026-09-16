#ifndef CASETERMINALSREPORTDIALOG_H
#define CASETERMINALSREPORTDIALOG_H

#include <QDialog>

class QSqlQueryModel;
class QComboBox;

// Отчёт «Терминалы с чехлами»: терминал, модель, статус, наличие чехла,
// тип чехла. Фильтр: все / с чехлом / без чехла. Экспорт в Excel.
class CaseTerminalsReportDialog : public QDialog {
    Q_OBJECT

public:
    explicit CaseTerminalsReportDialog(QWidget* parent = nullptr);

private slots:
    void applyFilter();
    void exportReport();

private:
    QSqlQueryModel* m_model = nullptr;
    QComboBox* m_filterCombo = nullptr;
};

#endif // CASETERMINALSREPORTDIALOG_H