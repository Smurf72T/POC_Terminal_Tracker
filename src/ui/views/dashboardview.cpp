#include "views/dashboardview.h"
#include "ui_mainwindow.h"
#include "views/chartpanel.h"

#include "database/databasemanager.h"

#include <QDateTime>
#include <QHeaderView>
#include <QLabel>
#include <QModelIndex>
#include <QSqlQuery>
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
    QSqlQuery query(DatabaseManager::instance().getDatabase());

    if (query.exec("SELECT COUNT(*) FROM tblterminals") && query.next()) {
        updateCounterWidget(m_ui->labelValueTotal, m_ui->labelNameTotal, query.value(0).toString(), "Всего терминалов",
                            "#3498db");
    }

    if (query.exec("SELECT COUNT(*) FROM tblterminals WHERE status = 0") && query.next()) {
        updateCounterWidget(m_ui->labelValueFree, m_ui->labelNameFree, query.value(0).toString(), "Свободно терминалов",
                            "#2ecc71");
    }

    if (query.exec("SELECT COUNT(*) FROM tblterminals WHERE status = 1") && query.next()) {
        updateCounterWidget(m_ui->labelValueRented, m_ui->labelNameRented, query.value(0).toString(), "В аренде",
                            "#e74c3c");
    }

    if (query.exec("SELECT COUNT(*) FROM tblsimcards") && query.next()) {
        updateCounterWidget(m_ui->labelValueTotalSIM, m_ui->labelNameTotalSIM, query.value(0).toString(),
                            "Всего SIM-карт", "#9b59b6");
    }

    if (query.exec("SELECT COUNT(*) FROM tblsimcards s "
                   "WHERE s.status = 0 "
                   "OR EXISTS ("
                   "    SELECT 1 FROM tblterminals t "
                   "    WHERE t.currentsimcardid = s.simcardid "
                   "    AND t.status = 0"
                   ")") &&
        query.next()) {
        updateCounterWidget(m_ui->labelValueFreeSIM, m_ui->labelNameFreeSIM, query.value(0).toString(), "Свободно SIM",
                            "#1abc9c");
    }

    if (query.exec("SELECT COUNT(*) FROM tblclients") && query.next()) {
        updateCounterWidget(m_ui->labelValueClients, m_ui->labelNameClients, query.value(0).toString(), "Клиентов",
                            "#f39c12");
    }
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
    QString queryStr = "SELECT c.clientid, c.clientname AS \"Клиент\", "
                       "COUNT(t.terminalid) AS \"Терминалов в аренде\" "
                       "FROM tblclients c "
                       "JOIN tblrentaldocs r ON c.clientid = r.clientid "
                       "JOIN tblrentaldetails rd ON r.rentaldocid = rd.rentaldocid "
                       "JOIN tblterminals t ON rd.terminalid = t.terminalid AND t.status = 1 "
                       "GROUP BY c.clientid, c.clientname "
                       "ORDER BY COUNT(t.terminalid) DESC";

    m_topClientsModel->setQuery(queryStr, DatabaseManager::instance().getDatabase());
    m_ui->tableViewTopClients->hideColumn(0);
}

void DashboardView::loadRecentDocuments()
{
    QString queryStr = "SELECT 1 AS doctype, receiptdocid AS docid, docnumber AS \"Номер\", docdate AS \"Дата\", "
                       "'Поступление' AS \"Тип\" FROM tblreceiptdocs "
                       "UNION ALL "
                       "SELECT 2, rentaldocid, docnumber, docdate, 'Аренда' FROM tblrentaldocs "
                       "UNION ALL "
                       "SELECT 3, returndocid, docnumber, docdate, 'Возврат' FROM tblreturndocs "
                       "UNION ALL "
                       "SELECT 5, statuschangedocid, docnumber, docdate, 'Изменение статуса' FROM tblstatuschangedocs "
                       "ORDER BY \"Дата\" DESC "
                       "LIMIT 15";

    m_recentDocsModel->setQuery(queryStr, DatabaseManager::instance().getDatabase());
    m_ui->tableViewRecentDocs->hideColumn(0);
    m_ui->tableViewRecentDocs->hideColumn(1);
}

void DashboardView::updateLastUpdateLabel()
{
    m_ui->labelLastUpdate->setText("Последнее обновление: " +
                                   QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss"));
}