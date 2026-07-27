#include "dashboardform.h"
#include "ui_dashboardform.h"
#include "../database/databasemanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>
#include <QDateTime>
#include <QDebug>

DashboardForm::DashboardForm(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DashboardForm),
    topClientsModel(new QSqlQueryModel(this)),
    recentDocsModel(new QSqlQueryModel(this)),
    refreshTimer(new QTimer(this))
{
    ui->setupUi(this);
    setWindowTitle("Дашборд — Остатки и статистика");
    resize(1200, 800);

    setupUI();

    // Настраиваем автообновление каждые 30 секунд
    connect(refreshTimer, &QTimer::timeout, this, &DashboardForm::autoRefresh);
    refreshTimer->start(30000); // 30 секунд

    // Первичная загрузка
    on_btnRefresh_clicked();
}

DashboardForm::~DashboardForm()
{
    delete ui;
}

void DashboardForm::setupUI()
{
    // Настраиваем таблицы
    ui->tableViewTopClients->setModel(topClientsModel);
    ui->tableViewTopClients->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableViewTopClients->setAlternatingRowColors(true);
    ui->tableViewTopClients->horizontalHeader()->setStretchLastSection(true);

    ui->tableViewRecentDocs->setModel(recentDocsModel);
    ui->tableViewRecentDocs->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableViewRecentDocs->setAlternatingRowColors(true);
    ui->tableViewRecentDocs->horizontalHeader()->setStretchLastSection(true);

    // Обновляем метку времени
    ui->labelLastUpdate->setText("Последнее обновление: —");
}

void DashboardForm::loadCounters()
{
    QSqlQuery query(DatabaseManager::instance().getDatabase());

    // Всего терминалов
    if (query.exec("SELECT COUNT(*) FROM tblterminals") && query.next()) {
        updateCounterWidget(ui->frameTotalTerminals,
            query.value(0).toString(), "Всего терминалов", "#3498db");
    }

    // Свободных терминалов
    if (query.exec("SELECT COUNT(*) FROM tblterminals WHERE status = 0") && query.next()) {
        updateCounterWidget(ui->frameFreeTerminals,
            query.value(0).toString(), "Свободно", "#2ecc71");
    }

    // В аренде
    if (query.exec("SELECT COUNT(*) FROM tblterminals WHERE status = 1") && query.next()) {
        updateCounterWidget(ui->frameRentedTerminals,
            query.value(0).toString(), "В аренде", "#e74c3c");
    }

    // Всего SIM
    if (query.exec("SELECT COUNT(*) FROM tblsimcards") && query.next()) {
        updateCounterWidget(ui->frameTotalSIM,
            query.value(0).toString(), "Всего SIM-карт", "#9b59b6");
    }

    // Свободных SIM
    if (query.exec("SELECT COUNT(*) FROM tblsimcards WHERE status = 0") && query.next()) {
        updateCounterWidget(ui->frameFreeSIM,
            query.value(0).toString(), "Свободно SIM", "#1abc9c");
    }

    // Клиентов
    if (query.exec("SELECT COUNT(*) FROM tblclients") && query.next()) {
        updateCounterWidget(ui->frameClients,
            query.value(0).toString(), "Клиентов", "#f39c12");
    }
}

void DashboardForm::updateCounterWidget(QWidget* widget, const QString& value,
                                         const QString& label, const QString& color)
{
    // Ищем QLabel внутри фрейма по уникальным именам
    QLabel* valueLabel = nullptr;
    for (auto* child : widget->findChildren<QLabel*>()) {
        if (child->objectName().endsWith("Value")) {
            valueLabel = child;
            break;
        }
    }
    QLabel* nameLabel = nullptr;
    for (auto* child : widget->findChildren<QLabel*>()) {
        if (child->objectName().endsWith("Name")) {
            nameLabel = child;
            break;
        }
    }

    if (valueLabel) {
        valueLabel->setText(value);
        valueLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 32px; font-weight: bold; }").arg(color));
    }
    if (nameLabel) {
        nameLabel->setText(label);
    }
}

void DashboardForm::loadTopClients()
{
    QString queryStr =
        "SELECT c.clientname AS \"Клиент\", "
        "COUNT(t.terminalid) AS \"Терминалов в аренде\" "
        "FROM tblclients c "
        "JOIN tblrentaldocs r ON c.clientid = r.clientid "
        "JOIN tblrentaldetails rd ON r.rentaldocid = rd.rentaldocid "
        "JOIN tblterminals t ON rd.terminalid = t.terminalid AND t.status = 1 "
        "GROUP BY c.clientid, c.clientname "
        "ORDER BY COUNT(t.terminalid) DESC "
        "LIMIT 10";

    topClientsModel->setQuery(queryStr, DatabaseManager::instance().getDatabase());

    if (topClientsModel->lastError().isValid()) {
        qDebug() << "Ошибка загрузки топ клиентов:" << topClientsModel->lastError().text();
    }

    ui->tableViewTopClients->resizeColumnsToContents();
}

void DashboardForm::loadRecentDocuments()
{
    QString queryStr =
        "SELECT docnumber AS \"Номер\", docdate AS \"Дата\", 'Поступление' AS \"Тип\" FROM tblreceiptdocs "
        "UNION ALL "
        "SELECT docnumber, docdate, 'Аренда' FROM tblrentaldocs "
        "UNION ALL "
        "SELECT docnumber, docdate, 'Возврат' FROM tblreturndocs "
        "ORDER BY \"Дата\" DESC "
        "LIMIT 15";

    recentDocsModel->setQuery(queryStr, DatabaseManager::instance().getDatabase());

    if (recentDocsModel->lastError().isValid()) {
        qDebug() << "Ошибка загрузки последних документов:" << recentDocsModel->lastError().text();
    }

    ui->tableViewRecentDocs->resizeColumnsToContents();
}

void DashboardForm::on_btnRefresh_clicked()
{
    loadCounters();
    loadTopClients();
    loadRecentDocuments();

    ui->labelLastUpdate->setText("Последнее обновление: " +
        QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss"));
}

void DashboardForm::autoRefresh()
{
    on_btnRefresh_clicked();
}

void DashboardForm::onDataChanged()
{
    // Автоматически обновляем дашборд при изменениях данных в других формах
    on_btnRefresh_clicked();
}

void DashboardForm::on_btnClose_clicked()
{
    refreshTimer->stop();
    close();
}