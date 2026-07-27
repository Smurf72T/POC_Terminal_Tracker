#include "mainwindow.h"
#include "manufacturersform.h"
#include "modelsform.h"
#include "ui_mainwindow.h"
#include "../database/databasemanager.h"
#include <QMessageBox>
#include <QDateTime>
#include "modelsform.h"
#include "clientsform.h"
#include "simcardsform.h"
#include "terminalsform.h"
#include <QTimer>
#include "receiptform.h"
#include "rentalform.h"
#include "returnform.h"
#include "archivedocumentsform.h"
#include "paymentform.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setupUI();
    updateStatusBar();
    connect(ui->actionManufacturers, &QAction::triggered, this, &MainWindow::onActionManufacturers_triggered);
    connect(ui->actionClients, &QAction::triggered, this, &MainWindow::onActionClients_triggered);
    connect(ui->actionSIMCards, &QAction::triggered, this, &MainWindow::onActionSIMCards_triggered);
    connect(ui->actionReceipt, &QAction::triggered, this, &MainWindow::onActionReceipt_triggered);
    connect(ui->actionRental, &QAction::triggered, this, &MainWindow::onActionRental_triggered);
    connect(ui->actionReturn, &QAction::triggered, this, &MainWindow::onActionReturn_triggered);
    connect(ui->actionArchiveReceipt, &QAction::triggered, this, &MainWindow::onActionArchiveReceipt_triggered);
    connect(ui->actionArchiveRental, &QAction::triggered, this, &MainWindow::onActionArchiveRental_triggered);
    connect(ui->actionArchiveReturn, &QAction::triggered, this, &MainWindow::onActionArchiveReturn_triggered);
    connect(ui->actionArchivePayment, &QAction::triggered, this, &MainWindow::onActionArchivePayment_triggered);
    connect(ui->actionPayment, &QAction::triggered, this, &MainWindow::onActionPayment_triggered);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupUI()
{
    setWindowTitle("POC Terminal Tracker");
    resize(1024, 768);

    // Подключаем сигналы меню
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::onActionAbout_triggered);
    connect(ui->actionExit, &QAction::triggered, this, &MainWindow::onActionExit_triggered);
    connect(ui->actionModels, &QAction::triggered, this, &MainWindow::onActionModels_triggered);
    connect(ui->actionTerminals, &QAction::triggered, this, &MainWindow::onActionTerminals_triggered);
}

void MainWindow::updateStatusBar()
{
    QString statusText = QString("Подключено к БД: ") +
                         (DatabaseManager::instance().isConnected() ? "Да" : "Нет") +
                         " | " + QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss");
    statusBar()->showMessage(statusText);
}

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
    ManufacturersForm *form = new ManufacturersForm(this);
    form->setWindowModality(Qt::WindowModal);
    form->show();
    QTimer::singleShot(50, this, [this, form]() { centerWindow(form); });
}

void MainWindow::onActionModels_triggered()
{
    ModelsForm *form = new ModelsForm(this);
    form->setWindowModality(Qt::WindowModal);
    form->show();
    QTimer::singleShot(50, this, [this, form]() { centerWindow(form); });
}

void MainWindow::onActionClients_triggered()
{
    ClientsForm *form = new ClientsForm(this);
    form->setWindowModality(Qt::WindowModal);
    form->show();
    QTimer::singleShot(50, this, [this, form]() { centerWindow(form); });
}

void MainWindow::onActionSIMCards_triggered()
{
    SIMCardsForm *form = new SIMCardsForm(this);
    form->setWindowModality(Qt::WindowModal);
    form->show();
    QTimer::singleShot(50, this, [this, form]() { centerWindow(form); });
}

void MainWindow::onActionTerminals_triggered()
{
    TerminalsForm *form = new TerminalsForm(this);
    form->setWindowModality(Qt::WindowModal);
    form->show();
    QTimer::singleShot(50, this, [this, form]() { centerWindow(form); });
}

void MainWindow::centerWindow(QWidget *widget)
{
    if (!widget || !this) return;

    QRect mainGeometry = this->geometry();
    QRect widgetGeometry = widget->geometry();

    int x = mainGeometry.x() + (mainGeometry.width() - widgetGeometry.width()) / 2;
    int y = mainGeometry.y() + (mainGeometry.height() - widgetGeometry.height()) / 2;

    widget->move(x, y);
}

void MainWindow::onActionReceipt_triggered()
{
    ReceiptForm *form = new ReceiptForm(this);
    form->show();
}

void MainWindow::onActionRental_triggered()
{
    RentalForm *form = new RentalForm(this);
    form->show();
}

void MainWindow::onActionReturn_triggered()
{
    ReturnForm *form = new ReturnForm(this);
    form->show();
}

void MainWindow::onActionArchiveReceipt_triggered()
{
    ArchiveDocumentsForm *form = new ArchiveDocumentsForm(1, this); // 1 = Поступление
    form->show();
}

void MainWindow::onActionArchiveRental_triggered()
{
    ArchiveDocumentsForm *form = new ArchiveDocumentsForm(2, this); // 2 = Аренда
    form->show();
}

void MainWindow::onActionArchiveReturn_triggered()
{
    ArchiveDocumentsForm *form = new ArchiveDocumentsForm(3, this); // 3 = Возврат
    form->show();
}

void MainWindow::onActionArchivePayment_triggered()
{
    ArchiveDocumentsForm *form = new ArchiveDocumentsForm(4, this); // 4 = Оплата
    form->show();
}

void MainWindow::onActionPayment_triggered()
{
    PaymentForm *form = new PaymentForm(this);
    form->show();
}