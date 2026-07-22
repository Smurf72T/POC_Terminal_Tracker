#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "../database/databasemanager.h"
#include <QMessageBox>
#include <QDateTime>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setupUI();
    updateStatusBar();
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