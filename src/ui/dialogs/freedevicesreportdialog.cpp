#include "dialogs/freedevicesreportdialog.h"

#include "database/databasemanager.h"
#include "database/repositories/caserepository.h"
#include "database/repositories/simcardrepository.h"
#include "database/repositories/terminalrepository.h"
#include "utils/reportexporter.h"

#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlQueryModel>
#include <QTableView>
#include <QVBoxLayout>

FreeDevicesReportDialog::FreeDevicesReportDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("Отчёт: Свободные терминалы, SIM-карты и чехлы");
    resize(900, 650);

    auto* layout = new QVBoxLayout(this);

    auto* termGroupBox = new QGroupBox("Свободные терминалы", this);
    auto* termLayout = new QVBoxLayout(termGroupBox);
    auto* termModel = new QSqlQueryModel(termGroupBox);
    m_termView = new QTableView(termGroupBox);

    TerminalRepository terminals(DatabaseManager::instance().getDatabase());
    terminals.populateFreeTerminals(termModel);
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

    SimCardRepository sims(DatabaseManager::instance().getDatabase());
    sims.populateFreeSimCards(simModel);
    m_simView->setModel(simModel);
    m_simView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_simView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_simView->horizontalHeader()->setStretchLastSection(true);
    m_simView->setAlternatingRowColors(true);
    m_simView->setColumnWidth(0, 250);
    m_simView->setColumnWidth(1, 300);
    simLayout->addWidget(m_simView);

    auto* caseGroupBox = new QGroupBox("Свободные чехлы", this);
    auto* caseLayout = new QVBoxLayout(caseGroupBox);
    auto* caseModel = new QSqlQueryModel(caseGroupBox);
    m_caseView = new QTableView(caseGroupBox);

    CaseRepository cases(DatabaseManager::instance().getDatabase());
    cases.populateFreeCasesSummary(caseModel);
    m_caseView->setModel(caseModel);
    m_caseView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_caseView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_caseView->horizontalHeader()->setStretchLastSection(true);
    m_caseView->setAlternatingRowColors(true);
    m_caseView->setColumnWidth(0, 250);
    caseLayout->addWidget(m_caseView);

    auto* btnLayout = new QHBoxLayout();
    auto* btnExportTerm = new QPushButton("Экспорт терминалов в Excel", this);
    auto* btnExportSim = new QPushButton("Экспорт SIM-карт в Excel", this);
    auto* btnExportCase = new QPushButton("Экспорт чехлов в Excel", this);
    auto* btnClose = new QPushButton("Закрыть", this);
    btnLayout->addWidget(btnExportTerm);
    btnLayout->addWidget(btnExportSim);
    btnLayout->addWidget(btnExportCase);
    btnLayout->addStretch();
    btnLayout->addWidget(btnClose);

    layout->addWidget(termGroupBox);
    layout->addWidget(simGroupBox);
    layout->addWidget(caseGroupBox);
    layout->addLayout(btnLayout);

    connect(btnExportTerm, &QPushButton::clicked, this, &FreeDevicesReportDialog::exportTerminals);
    connect(btnExportSim, &QPushButton::clicked, this, &FreeDevicesReportDialog::exportSimCards);
    connect(btnExportCase, &QPushButton::clicked, this, &FreeDevicesReportDialog::exportCases);
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
        if (ReportExporter::exportModelToExcel(model, "Свободные терминалы", filePath, this))
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
        if (ReportExporter::exportModelToExcel(model, "Свободные SIM-карты", filePath, this))
            QMessageBox::information(this, "Успех", "SIM-карты экспортированы.");
    }
}

void FreeDevicesReportDialog::exportCases()
{
    auto* model = qobject_cast<QSqlQueryModel*>(m_caseView->model());
    if (!model)
        return;
    QString filePath = QFileDialog::getSaveFileName(this, "Экспорт свободных чехлов", "free_cases.xlsx",
                                                    "Excel (*.xlsx);;Все файлы (*)");
    if (!filePath.isEmpty()) {
        if (ReportExporter::exportModelToExcel(model, "Свободные чехлы", filePath, this))
            QMessageBox::information(this, "Успех", "Чехлы экспортированы.");
    }
}