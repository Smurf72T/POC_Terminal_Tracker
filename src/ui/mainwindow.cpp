#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "database/databasemanager.h"
#include "manufacturersform.h"
#include "modelsform.h"
#include "clientsform.h"
#include "simcardsform.h"
#include "terminalsform.h"
#include "receiptform.h"
#include "rentalform.h"
#include "returnform.h"
#include "paymentform.h"
#include "archivedocumentsform.h"
#include <QMessageBox>
#include <QDateTime>
#include <QSqlQuery>
#include <QSqlError>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    topClientsModel(new QSqlQueryModel(this)),
    recentDocsModel(new QSqlQueryModel(this)),
    refreshTimer(new QTimer(this))
{
    ui->setupUi(this);
    setupUI();
    updateStatusBar();

    // Подключаем сигнал от DatabaseManager
    connect(&DatabaseManager::instance(), &DatabaseManager::dataChanged,
            this, &MainWindow::onDatabaseDataChanged);

    // Настраиваем таблицы
    ui->tableViewTopClients->setModel(topClientsModel);
    ui->tableViewTopClients->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableViewTopClients->setAlternatingRowColors(true);
    ui->tableViewTopClients->horizontalHeader()->setStretchLastSection(true);

    ui->tableViewRecentDocs->setModel(recentDocsModel);
    ui->tableViewRecentDocs->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableViewRecentDocs->setAlternatingRowColors(true);
    ui->tableViewRecentDocs->horizontalHeader()->setStretchLastSection(true);

    // Автообновление каждые 30 секунд
    connect(refreshTimer, &QTimer::timeout, this, [this]() {
        loadCounters();
        loadTopClients();
        loadRecentDocuments();
        ui->labelLastUpdate->setText("Последнее обновление: " +
            QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss"));
    });
    refreshTimer->start(30000);

    // Первичная загрузка
    loadCounters();
    loadTopClients();
    loadRecentDocuments();
    ui->labelLastUpdate->setText("Последнее обновление: " +
        QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss"));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupUI()
{
    setWindowTitle("POC Terminal Tracker");
    resize(1200, 800);

    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::onActionAbout_triggered);
    connect(ui->actionExit, &QAction::triggered, this, &MainWindow::onActionExit_triggered);
    connect(ui->actionManufacturers, &QAction::triggered, this, &MainWindow::onActionManufacturers_triggered);
    connect(ui->actionModels, &QAction::triggered, this, &MainWindow::onActionModels_triggered);
    connect(ui->actionClients, &QAction::triggered, this, &MainWindow::onActionClients_triggered);
    connect(ui->actionSIMCards, &QAction::triggered, this, &MainWindow::onActionSIMCards_triggered);
    connect(ui->actionTerminals, &QAction::triggered, this, &MainWindow::onActionTerminals_triggered);
    connect(ui->actionReceipt, &QAction::triggered, this, &MainWindow::onActionReceipt_triggered);
    connect(ui->actionRental, &QAction::triggered, this, &MainWindow::onActionRental_triggered);
    connect(ui->actionReturn, &QAction::triggered, this, &MainWindow::onActionReturn_triggered);
    connect(ui->actionPayment, &QAction::triggered, this, &MainWindow::onActionPayment_triggered);
    connect(ui->actionArchiveReceipt, &QAction::triggered, this, &MainWindow::onActionArchiveReceipt_triggered);
    connect(ui->actionArchiveRental, &QAction::triggered, this, &MainWindow::onActionArchiveRental_triggered);
    connect(ui->actionArchiveReturn, &QAction::triggered, this, &MainWindow::onActionArchiveReturn_triggered);
}

void MainWindow::updateStatusBar()
{
    QString statusText = QString("Подключено к БД: ") +
                         (DatabaseManager::instance().isConnected() ? "Да" : "Нет") +
                         " | " + QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss");
    statusBar()->showMessage(statusText);
}

void MainWindow::loadCounters()
{
    QSqlQuery query(DatabaseManager::instance().getDatabase());

    if (query.exec("SELECT COUNT(*) FROM tblterminals") && query.next()) {
        updateCounterWidget(ui->frameTotalTerminals,
            query.value(0).toString(), "Всего терминалов", "#3498db");
    }

    if (query.exec("SELECT COUNT(*) FROM tblterminals WHERE status = 0") && query.next()) {
        updateCounterWidget(ui->frameFreeTerminals,
            query.value(0).toString(), "Свободно терминалов", "#2ecc71");
    }

    if (query.exec("SELECT COUNT(*) FROM tblterminals WHERE status = 1") && query.next()) {
        updateCounterWidget(ui->frameRentedTerminals,
            query.value(0).toString(), "В аренде", "#e74c3c");
    }

    if (query.exec("SELECT COUNT(*) FROM tblsimcards") && query.next()) {
        updateCounterWidget(ui->frameTotalSIM,
            query.value(0).toString(), "Всего SIM-карт", "#9b59b6");
    }

    // ИСПРАВЛЕНО: SIM-карта считается свободной, если:
    // 1. status = 0, ИЛИ
    // 2. status = 1 (в аренде), но терминал с этой SIM уже возвращен (status = 0)
    if (query.exec(
        "SELECT COUNT(*) FROM tblsimcards s "
        "WHERE s.status = 0 "
        "OR EXISTS (" 
        "    SELECT 1 FROM tblterminals t "
        "    WHERE t.currentsimcardid = s.simcardid "
        "    AND t.status = 0"
        ")"
    ) && query.next()) {
        updateCounterWidget(ui->frameFreeSIM,
            query.value(0).toString(), "Свободно SIM", "#1abc9c");
    }

    if (query.exec("SELECT COUNT(*) FROM tblclients") && query.next()) {
        updateCounterWidget(ui->frameClients,
            query.value(0).toString(), "Клиентов", "#f39c12");
    }
}

void MainWindow::updateCounterWidget(QWidget* widget, const QString& value,
                                     const QString& label, const QString& color)
{
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
        nameLabel->setStyleSheet("QLabel { color: #E0E0E0; font-size: 14px; font-weight: bold; }");
    }
}

void MainWindow::openForm(QWidget *form)
{
    form->setAttribute(Qt::WA_DeleteOnClose);
    form->setWindowModality(Qt::WindowModal);
    form->setWindowFlags(form->windowFlags() | Qt::Window);
    form->show();

    // Центрирование относительно главного окна
    QRect mainRect = this->geometry();
    QRect formRect = form->frameGeometry();
    form->move(mainRect.center() - formRect.center());
}

void MainWindow::loadTopClients()
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
    ui->tableViewTopClients->resizeColumnsToContents();
}

void MainWindow::loadRecentDocuments()
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
    ui->tableViewRecentDocs->resizeColumnsToContents();
}

void MainWindow::onDatabaseDataChanged()
{
    // Автоматическое обновление при изменении данных
    loadCounters();
    loadTopClients();
    loadRecentDocuments();
    ui->labelLastUpdate->setText("Последнее обновление: " +
        QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss"));
}

// ... все остальные слоты (onActionManufacturers_triggered и т.д.) остаются без изменений ...

void MainWindow::onActionAbout_triggered()
{
    QMessageBox::about(this, "О программе",
                       "POC Terminal Tracker\n"
                       "Версия 1.0.0\n\n"
                       "Система учёта POC-терминалов и SIM-карт");
}

void MainWindow::onActionExit_triggered()
{
    DatabaseManager::instance().close();
    QCoreApplication::quit();
}

void MainWindow::onActionManufacturers_triggered()
{
    openForm(new ManufacturersForm(this));
}

void MainWindow::onActionModels_triggered()
{
    openForm(new ModelsForm(this));
}

void MainWindow::onActionClients_triggered()
{
    openForm(new ClientsForm(this));
}

void MainWindow::onActionSIMCards_triggered()
{
    openForm(new SIMCardsForm(this));
}

void MainWindow::onActionTerminals_triggered()
{
    openForm(new TerminalsForm(this));
}

void MainWindow::onActionReceipt_triggered()
{
    openForm(new ReceiptForm(this));
}

void MainWindow::onActionRental_triggered()
{
    openForm(new RentalForm(this));
}

void MainWindow::onActionReturn_triggered()
{
    openForm(new ReturnForm(this));
}

void MainWindow::onActionPayment_triggered()
{
    openForm(new PaymentForm(this));
}

void MainWindow::onActionArchiveReceipt_triggered()
{
    openForm(new ArchiveDocumentsForm(1, this));
}

void MainWindow::onActionArchiveRental_triggered()
{
    openForm(new ArchiveDocumentsForm(2, this));
}

void MainWindow::onActionArchiveReturn_triggered()
{
    openForm(new ArchiveDocumentsForm(3, this));
}