#include "views/dashboardview.h"
#include "ui_mainwindow.h"
#include "views/chartpanel.h"

#include "database/databasemanager.h"
#include "database/repositories/clientrepository.h"
#include "database/repositories/documentrepository.h"
#include "database/repositories/simcardrepository.h"
#include "database/repositories/terminalrepository.h"

#include <QDateTime>
#include <QHeaderView>
#include <QLabel>
#include <QModelIndex>
#include <QSqlQueryModel>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

DashboardView::DashboardView(Ui::MainWindow* ui, QWidget* centralWidget, QObject* parent) :
    QObject(parent), m_ui(ui), m_topClientsModel(new QSqlQueryModel(this)), m_recentDocsModel(new QSqlQueryModel(this)),
    m_refreshTimer(new QTimer(this))
{
    setupTables();
    setupCharts(centralWidget);

    connect(m_refreshTimer, &QTimer::timeout, this, &DashboardView::refreshAll);
    connect(m_refreshTimer, &QTimer::timeout, this, &DashboardView::refreshCharts);
    m_refreshTimer->start(30000);

    connect(&DatabaseManager::instance(), &DatabaseManager::dataChanged, this, &DashboardView::onDatabaseDataChanged);

    connect(m_ui->tableViewRecentDocs, &QTableView::doubleClicked, this, &DashboardView::onRecentDocDoubleClicked);
    connect(m_ui->tableViewTopClients, &QTableView::doubleClicked, this, &DashboardView::onTopClientDoubleClicked);

    refreshAll();
}

void DashboardView::setupTables()
{
    m_ui->tableViewTopClients->setModel(m_topClientsModel);
    m_ui->tableViewTopClients->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ui->tableViewTopClients->setAlternatingRowColors(true);
    m_ui->tableViewTopClients->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_ui->tableViewTopClients->verticalHeader()->setDefaultSectionSize(40);

    m_ui->tableViewRecentDocs->setModel(m_recentDocsModel);
    m_ui->tableViewRecentDocs->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ui->tableViewRecentDocs->setAlternatingRowColors(true);
    m_ui->tableViewRecentDocs->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_ui->tableViewRecentDocs->verticalHeader()->setDefaultSectionSize(40);
}

void DashboardView::setupCharts(QWidget* centralWidget)
{
    m_charts = new ChartPanel(centralWidget);
    auto* mainLayout = qobject_cast<QVBoxLayout*>(centralWidget->layout());
    if (mainLayout) {
        int idx = mainLayout->indexOf(m_ui->groupBoxTopClients);
        if (idx >= 0)
            mainLayout->insertWidget(idx, m_charts);
        else
            mainLayout->addWidget(m_charts);
    }
}

void DashboardView::refreshAll()
{
    loadCounters();
    loadTopClients();
    loadRecentDocuments();
    updateLastUpdateLabel();
}

void DashboardView::refreshCharts()
{
    m_charts->refresh();
}

void DashboardView::applyDarkTheme(bool dark)
{
    m_charts->applyDarkTheme(dark);
}

int DashboardView::recentDocTypeAt(const QModelIndex& index) const
{
    return m_recentDocsModel->data(m_recentDocsModel->index(index.row(), 0)).toInt();
}

int DashboardView::recentDocIdAt(const QModelIndex& index) const
{
    return m_recentDocsModel->data(m_recentDocsModel->index(index.row(), 1)).toInt();
}

int DashboardView::topClientIdAt(const QModelIndex& index) const
{
    return m_topClientsModel->data(m_topClientsModel->index(index.row(), 0)).toInt();
}

QString DashboardView::topClientNameAt(const QModelIndex& index) const
{
    return m_topClientsModel->data(m_topClientsModel->index(index.row(), 1)).toString();
}

void DashboardView::onDatabaseDataChanged()
{
    refreshAll();
}

void DashboardView::onRecentDocDoubleClicked(const QModelIndex& index)
{
    int docType = recentDocTypeAt(index);
    int docId = recentDocIdAt(index);
    if (docId <= 0)
        return;
    emit recentDocActivated(docType, docId);
    refreshAll();
}

void DashboardView::onTopClientDoubleClicked(const QModelIndex& index)
{
    int clientId = topClientIdAt(index);
    if (clientId <= 0)
        return;
    QString clientName = topClientNameAt(index);
    emit topClientActivated(clientId, clientName);
}

void DashboardView::loadCounters()
{
    const QSqlDatabase& db = DatabaseManager::instance().getDatabase();
    TerminalRepository terminals(db);
    SimCardRepository sims(db);
    ClientRepository clients(db);

    updateCounterWidget(m_ui->labelValueTotal, m_ui->labelNameTotal, QString::number(terminals.countAll()),
                        "Всего терминалов", "#3498db");
    updateCounterWidget(m_ui->labelValueFree, m_ui->labelNameFree, QString::number(terminals.countByStatus(0)),
                        "Свободно терминалов", "#2ecc71");
    updateCounterWidget(m_ui->labelValueRented, m_ui->labelNameRented, QString::number(terminals.countByStatus(1)),
                        "В аренде", "#e74c3c");
    updateCounterWidget(m_ui->labelValueTotalSIM, m_ui->labelNameTotalSIM, QString::number(sims.countAll()),
                        "Всего SIM-карт", "#9b59b6");
    updateCounterWidget(m_ui->labelValueFreeSIM, m_ui->labelNameFreeSIM, QString::number(sims.countFree()),
                        "Свободно SIM", "#1abc9c");
    updateCounterWidget(m_ui->labelValueClients, m_ui->labelNameClients, QString::number(clients.countAll()),
                        "Клиентов", "#f39c12");
}

void DashboardView::updateCounterWidget(QLabel* valueLabel, QLabel* nameLabel, const QString& value,
                                        const QString& label, const QString& color)
{
    if (valueLabel) {
        valueLabel->setText(value);
        valueLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 32px; font-weight: bold; }").arg(color));
    }
    if (nameLabel) {
        nameLabel->setText(label);
        nameLabel->setStyleSheet("QLabel { color: #E0E0E0; font-size: 14px; font-weight: bold; }");
    }
}

void DashboardView::loadTopClients()
{
    ClientRepository clients(DatabaseManager::instance().getDatabase());
    clients.populateRentalStatistics(m_topClientsModel);
    m_ui->tableViewTopClients->hideColumn(0);
}

void DashboardView::loadRecentDocuments()
{
    DocumentRepository documents(DatabaseManager::instance().getDatabase());
    documents.populateRecentDocuments(m_recentDocsModel);
    m_ui->tableViewRecentDocs->hideColumn(0);
    m_ui->tableViewRecentDocs->hideColumn(1);
}

void DashboardView::updateLastUpdateLabel()
{
    m_ui->labelLastUpdate->setText("Последнее обновление: " +
                                   QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss"));
}