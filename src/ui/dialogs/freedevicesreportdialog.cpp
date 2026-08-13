#include "dialogs/freedevicesreportdialog.h"

#include "database/databasemanager.h"
#include "utils/logging.h"
#include "utils/reportexporter.h"

#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QTableView>
#include <QVBoxLayout>

FreeDevicesReportDialog::FreeDevicesReportDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("Отчёт: Свободные терминалы и SIM-карты");
    resize(900, 500);

    auto* layout = new QVBoxLayout(this);

    auto* termGroupBox = new QGroupBox("Свободные терминалы", this);
    auto* termLayout = new QVBoxLayout(termGroupBox);
    auto* termModel = new QSqlQueryModel(termGroupBox);
    m_termView = new QTableView(termGroupBox);

    QString termQuery = "SELECT t.serialnumber, m.modelname, "
                        "COALESCE(s.simnumber, 'SIM не назначена') AS simstatus "
                        "FROM tblterminals t "
                        "LEFT JOIN tblmodels m ON t.modelid = m.modelid "
                        "LEFT JOIN tblsimcards s ON t.currentsimcardid = s.simcardid "
                        "WHERE t.status = 0 "
                        "ORDER BY t.serialnumber";
    termModel->setQuery(termQuery, DatabaseManager::instance().getDatabase());
    m_termView->setModel(termModel);
    m_termView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_termView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_termView->horizontalHeader()->setStretchLastSection(true);
    m_termView->setAlternatingRowColors(true);
    m_termView->setColumnWidth(0, 200);
    m_termView->setColumnWidth(1, 200);
    m_termView->setColumnWidth(2, 200);
    termLayout->addWidget(m_termView);

    auto* simGroupBox = new QGroupBox("Свободные SIM-карты", this);
    auto* simLayout = new QVBoxLayout(simGroupBox);
    auto* simModel = new QSqlQueryModel(simGroupBox);
    m_simView = new QTableView(simGroupBox);

    QString simQuery = "SELECT s.simnumber, s.notes, "
                       "'Не используется' AS status "
                       "FROM tblsimcards s "
                       "WHERE s.status = 0 "
                       "AND s.simcardid NOT IN ("
                       "    SELECT t.currentsimcardid FROM tblterminals t WHERE t.currentsimcardid IS NOT NULL"
                       ") "
                       "ORDER BY s.simnumber";
    simModel->setQuery(simQuery, DatabaseManager::instance().getDatabase());
    m_simView->setModel(simModel);
    m_simView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_simView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_simView->horizontalHeader()->setStretchLastSection(true);
    m_simView->setAlternatingRowColors(true);
    m_simView->setColumnWidth(0, 250);
    m_simView->setColumnWidth(1, 300);
    simLayout->addWidget(m_simView);

    auto* btnLayout = new QHBoxLayout();
    auto* btnExportTerm = new QPushButton("Экспорт терминалов в Excel", this);
    auto* btnExportSim = new QPushButton("Экспорт SIM-карт в Excel", this);
    auto* btnClose = new QPushButton("Закрыть", this);
    btnLayout->addWidget(btnExportTerm);
    btnLayout->addWidget(btnExportSim);
    btnLayout->addStretch();
    btnLayout->addWidget(btnClose);

    layout->addWidget(termGroupBox);
    layout->addWidget(simGroupBox);
    layout->addLayout(btnLayout);

    connect(btnExportTerm, &QPushButton::clicked, this, &FreeDevicesReportDialog::exportTerminals);
    connect(btnExportSim, &QPushButton::clicked, this, &FreeDevicesReportDialog::exportSimCards);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
}

void FreeDevicesReportDialog::exportTerminals()
{
    auto* model = qobject_cast<QSqlQueryModel*>(m_termView->model());
    if (!model)
        return;
    QString filePath = QFileDialog::getSaveFileName(this, "Экспорт свободных терминалов", "free_terminals.xlsx",
                                                    "Excel (*.xlsx);;Все файлы (*)");
    if (!filePath.isEmpty()) {
        if (ReportExporter::exportModelToExcel(model, "Свободные терминалы", filePath))
            QMessageBox::information(this, "Успех", "Терминалы экспортированы.");
    }
}

void FreeDevicesReportDialog::exportSimCards()
{
    auto* model = qobject_cast<QSqlQueryModel*>(m_simView->model());
    if (!model)
        return;
    QString filePath = QFileDialog::getSaveFileName(this, "Экспорт свободных SIM", "free_simcards.xlsx",
                                                    "Excel (*.xlsx);;Все файлы (*)");
    if (!filePath.isEmpty()) {
        if (ReportExporter::exportModelToExcel(model, "Свободные SIM-карты", filePath))
            QMessageBox::information(this, "Успех", "SIM-карты экспортированы.");
    }
}