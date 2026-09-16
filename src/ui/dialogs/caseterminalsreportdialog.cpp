#include "dialogs/caseterminalsreportdialog.h"

#include "database/databasemanager.h"
#include "utils/reportexporter.h"

#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QTableView>
#include <QVBoxLayout>

CaseTerminalsReportDialog::CaseTerminalsReportDialog(QWidget* parent) :
    QDialog(parent), m_model(new QSqlQueryModel(this))
{
    setWindowTitle("Отчёт: Терминалы с чехлами");
    resize(900, 500);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto* filterLayout = new QHBoxLayout();
    filterLayout->addWidget(new QLabel("Фильтр:", this));
    m_filterCombo = new QComboBox(this);
    m_filterCombo->addItem("Все терминалы", 0);
    m_filterCombo->addItem("С чехлом", 1);
    m_filterCombo->addItem("Без чехла", 2);
    filterLayout->addWidget(m_filterCombo);
    auto* btnFilter = new QPushButton("Применить", this);
    filterLayout->addWidget(btnFilter);
    filterLayout->addStretch();
    layout->addLayout(filterLayout);

    auto* tableView = new QTableView(this);
    tableView->setModel(m_model);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView->setAlternatingRowColors(true);
    tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableView->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(tableView);

    auto* btnLayout = new QHBoxLayout();
    auto* btnExport = new QPushButton("Экспорт в Excel", this);
    auto* btnClose = new QPushButton("Закрыть", this);
    btnLayout->addWidget(btnExport);
    btnLayout->addStretch();
    btnLayout->addWidget(btnClose);
    layout->addLayout(btnLayout);

    connect(btnFilter, &QPushButton::clicked, this, &CaseTerminalsReportDialog::applyFilter);
    connect(m_filterCombo, &QComboBox::currentIndexChanged, this, &CaseTerminalsReportDialog::applyFilter);
    connect(btnExport, &QPushButton::clicked, this, &CaseTerminalsReportDialog::exportReport);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);

    applyFilter();
}

void CaseTerminalsReportDialog::applyFilter()
{
    QString queryStr = "SELECT t.serialnumber AS \"Терминал\", "
                       "COALESCE(m.modelname, '') AS \"Модель\", "
                       "CASE t.status WHEN 0 THEN 'Свободен' WHEN 1 THEN 'В аренде' "
                       "WHEN 2 THEN 'В ремонте/списан' ELSE CAST(t.status AS TEXT) END AS \"Статус\", "
                       "CASE WHEN t.currentcaseid IS NOT NULL THEN 'Да' ELSE 'Нет' END AS \"Чехол\", "
                       "COALESCE(c.casetype, '') AS \"Тип чехла\" "
                       "FROM tblterminals t "
                       "LEFT JOIN tblmodels m ON t.modelid = m.modelid "
                       "LEFT JOIN tblcases c ON t.currentcaseid = c.caseid ";

    const int filter = m_filterCombo->currentData().toInt();
    if (filter == 1)
        queryStr += "WHERE t.currentcaseid IS NOT NULL ";
    else if (filter == 2)
        queryStr += "WHERE t.currentcaseid IS NULL ";

    queryStr += "ORDER BY t.serialnumber";

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.exec(queryStr);
    m_model->setQuery(std::move(query));
}

void CaseTerminalsReportDialog::exportReport()
{
    QString filePath = QFileDialog::getSaveFileName(this, "Экспорт отчёта", "case_terminals.xlsx",
                                                    "Excel (*.xlsx);;Все файлы (*)");
    if (!filePath.isEmpty()) {
        if (ReportExporter::exportModelToExcel(m_model, "Терминалы с чехлами", filePath, this))
            QMessageBox::information(this, "Успех", "Отчёт экспортирован.");
    }
}