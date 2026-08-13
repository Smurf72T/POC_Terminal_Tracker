#include "dialogs/clientrentalreportdialog.h"

#include "database/databasemanager.h"
#include "database/repositories/clientrepository.h"
#include "utils/reportexporter.h"

#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlQueryModel>
#include <QTableView>
#include <QVBoxLayout>

ClientRentalReportDialog::ClientRentalReportDialog(int clientId, const QString& clientName, QWidget* parent) :
    QDialog(parent), m_model(new QSqlQueryModel(this)), m_clientName(clientName)
{
    QString title = QString("Клиент: %1 — Терминалы в аренде").arg(clientName);
    setWindowTitle(title);
    resize(900, 500);
    setStyleSheet("QDialog { background-color: #1E1E1E; }"
                  "QLabel#headerLabel { font-size: 18px; font-weight: bold; color: #FFFFFF; padding: 12px; }");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto* headerLabel = new QLabel(title, this);
    headerLabel->setObjectName("headerLabel");
    layout->addWidget(headerLabel);

    auto* groupBox = new QGroupBox("Арендованные терминалы", this);
    groupBox->setStyleSheet("QGroupBox { font-size: 13px; font-weight: bold; color: #CCCCCC; "
                            "border: 1px solid #3C3C3C; border-radius: 6px; margin-top: 8px; padding-top: 16px; }"
                            "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 6px; }");
    auto* groupLayout = new QVBoxLayout(groupBox);

    auto* tableView = new QTableView(groupBox);
    tableView->setModel(m_model);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView->setAlternatingRowColors(true);
    tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableView->verticalHeader()->hide();
    tableView->setStyleSheet("QTableView { background-color: #252526; alternate-background-color: #2A2A2A; "
                             "color: #E0E0E0; gridline-color: #333333; border: 1px solid #3C3C3C; border-radius: 4px; "
                             "selection-background-color: #1565C0; selection-color: white; }"
                             "QHeaderView::section { background: #2D2D2D; color: #FFFFFF; padding: 8px; "
                             "border: none; border-bottom: 2px solid #0D47A1; font-weight: bold; font-size: 12px; }");

    ClientRepository repo(DatabaseManager::instance().getDatabase());
    repo.populateRentedTerminals(m_model, clientId);

    groupLayout->addWidget(tableView);
    layout->addWidget(groupBox);

    auto* btnLayout = new QHBoxLayout();
    auto* btnExport = new QPushButton("Экспорт в Excel", this);
    btnExport->setStyleSheet("QPushButton { background-color: #1565C0; color: white; padding: 8px 20px; "
                             "border: none; border-radius: 4px; font-size: 13px; font-weight: bold; }"
                             "QPushButton:hover { background-color: #1976D2; }");
    auto* btnClose = new QPushButton("Закрыть", this);
    btnClose->setStyleSheet("QPushButton { background-color: #424242; color: white; padding: 8px 20px; "
                            "border: none; border-radius: 4px; font-size: 13px; }"
                            "QPushButton:hover { background-color: #616161; }");

    connect(btnExport, &QPushButton::clicked, this, &ClientRentalReportDialog::exportReport);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);

    btnLayout->addWidget(btnExport);
    btnLayout->addStretch();
    btnLayout->addWidget(btnClose);
    layout->addLayout(btnLayout);
}

void ClientRentalReportDialog::exportReport()
{
    QString filePath = QFileDialog::getSaveFileName(
        this, "Экспорт отчёта", QString("terminals_%1.xlsx").arg(m_clientName.simplified().replace(' ', '_')),
        "Excel (*.xlsx);;Все файлы (*)");
    if (!filePath.isEmpty()) {
        if (ReportExporter::exportModelToExcel(m_model, m_clientName, filePath))
            QMessageBox::information(this, "Успех", "Отчёт экспортирован.");
    }
}