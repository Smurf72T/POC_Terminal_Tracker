#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "database/databasemanager.h"
#include "dialogs/manufacturersform.h"
#include "dialogs/modelsform.h"
#include "dialogs/clientsform.h"
#include "dialogs/simcardsform.h"
#include "dialogs/terminalsform.h"
#include "dialogs/receiptform.h"
#include "dialogs/rentalform.h"
#include "dialogs/returnform.h"
#include "dialogs/paymentform.h"
#include "dialogs/archivedocumentsform.h"
#include "dialogs/terminalhistoryform.h"
#include "dialogs/bulkimportform.h"
#include "dialogs/auditlogform.h"
#include "dialogs/expirynotificationsform.h"
#include "dialogs/batchstatusform.h"
#include "dialogs/reportsform.h"
#include "dialogs/usermanagementform.h"
#include "utils/reportexporter.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlField>
#include <QLabel>
#include <QProcess>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableView>
#include <QSqlTableModel>
#include <QHeaderView>
#include <QTextDocument>
#include <QPrinter>
#include <QPrinterInfo>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QSqlRecord>
#include <QPushButton>
#include <QComboBox>
#include <QCompleter>
#include <QApplication>
#include <QFile>
#include <QShortcut>
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>

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
    ui->tableViewTopClients->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableViewTopClients->verticalHeader()->setDefaultSectionSize(40);

    connect(ui->tableViewTopClients, &QTableView::doubleClicked,
            this, &MainWindow::onTopClientDoubleClicked);

    ui->tableViewRecentDocs->setModel(recentDocsModel);
    ui->tableViewRecentDocs->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableViewRecentDocs->setAlternatingRowColors(true);
    ui->tableViewRecentDocs->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableViewRecentDocs->verticalHeader()->setDefaultSectionSize(40);

    connect(ui->tableViewRecentDocs, &QTableView::doubleClicked,
            this, &MainWindow::onRecentDocDoubleClicked);

    // Автообновление каждые 30 секунд
    connect(refreshTimer, &QTimer::timeout, this, [this]() {
        loadCounters();
        loadTopClients();
        loadRecentDocuments();
        ui->labelLastUpdate->setText("Последнее обновление: " +
            QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss"));
    });
    setupCharts();

    // Автообновление графиков
    connect(refreshTimer, &QTimer::timeout, this, &MainWindow::updateCharts);

    refreshTimer->start(30000);

    // Глобальный поиск Ctrl+K
    auto *searchShortcut = new QShortcut(QKeySequence("Ctrl+K"), this);
    connect(searchShortcut, &QShortcut::activated, this, &MainWindow::showGlobalSearch);

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

    auto *themeBtn = new QPushButton("☀️ Светлая тема", this);
    themeBtn->setFixedHeight(24);
    themeBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #E0E0E0; border: 1px solid #555; "
        "border-radius: 3px; padding: 2px 8px; font-size: 12px; }"
        "QPushButton:hover { background: #333; }"
    );
    statusBar()->addPermanentWidget(themeBtn);
    connect(themeBtn, &QPushButton::clicked, this, [this, themeBtn]() {
        m_darkTheme = !m_darkTheme;
        QFile file(m_darkTheme ? ":/styles/modern.qss" : ":/styles/light.qss");
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString style = QString::fromUtf8(file.readAll());
            qApp->setStyleSheet(style);
            file.close();
        }
        themeBtn->setText(m_darkTheme ? "☀️ Светлая тема" : "🌙 Тёмная тема");
        // Обновляем тему графиков
        if (chartStatusView && chartStatusView->chart())
            chartStatusView->chart()->setTheme(m_darkTheme ? QChart::ChartThemeDark : QChart::ChartThemeLight);
        if (chartRevenueView && chartRevenueView->chart())
            chartRevenueView->chart()->setTheme(m_darkTheme ? QChart::ChartThemeDark : QChart::ChartThemeLight);
        // Обновляем цвета кнопки под текущую тему
        themeBtn->setStyleSheet(
            QString("QPushButton { background: transparent; color: %1; border: 1px solid %2; "
                    "border-radius: 3px; padding: 2px 8px; font-size: 12px; }"
                    "QPushButton:hover { background: %3; }")
            .arg(m_darkTheme ? "#E0E0E0" : "#212121")
            .arg(m_darkTheme ? "#555" : "#999")
            .arg(m_darkTheme ? "#333" : "#DDD")
        );
        updateStatusBar();
    });

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
    connect(ui->actionArchivePayment, &QAction::triggered, this, &MainWindow::onActionArchivePayment_triggered);
    connect(ui->actionTerminalHistory, &QAction::triggered, this, &MainWindow::onActionTerminalHistory_triggered);
    connect(ui->actionFreeDevicesReport, &QAction::triggered, this, &MainWindow::onActionFreeDevicesReport_triggered);
    connect(ui->actionBulkImport, &QAction::triggered, this, &MainWindow::onActionBulkImport_triggered);
    connect(ui->actionBackup, &QAction::triggered, this, &MainWindow::onActionBackup_triggered);
    connect(ui->actionExpiryNotifications, &QAction::triggered, this, &MainWindow::onActionExpiryNotifications_triggered);
    connect(ui->actionAuditLog, &QAction::triggered, this, &MainWindow::onActionAuditLog_triggered);
    connect(ui->actionBatchStatus, &QAction::triggered, this, &MainWindow::onActionBatchStatus_triggered);
    connect(ui->actionReports, &QAction::triggered, this, &MainWindow::onActionReports_triggered);
    connect(ui->actionUserManagement, &QAction::triggered, this, &MainWindow::onActionUserManagement_triggered);
    connect(ui->actionGlobalSearch, &QAction::triggered, this, &MainWindow::showGlobalSearch);
}

void MainWindow::setupCharts()
{
    auto *chartsGroup = new QGroupBox("Аналитика", this);
    chartsGroup->setStyleSheet(
        "QGroupBox { font-size: 14px; font-weight: bold; color: #CCCCCC; "
        "border: 1px solid #3C3C3C; border-radius: 8px; margin-top: 8px; padding-top: 18px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; }"
    );
    auto *chartsLayout = new QHBoxLayout(chartsGroup);

    auto *pieChart = new QChart();
    pieChart->setTitle("Статус терминалов");
    pieChart->setTheme(QChart::ChartThemeDark);
    pieChart->setAnimationOptions(QChart::SeriesAnimations);
    pieChart->legend()->setAlignment(Qt::AlignBottom);
    chartStatusView = new QChartView(pieChart, chartsGroup);
    chartStatusView->setRenderHint(QPainter::Antialiasing);
    chartsLayout->addWidget(chartStatusView);

    auto *barChart = new QChart();
    barChart->setTitle("Выручка по месяцам");
    barChart->setTheme(QChart::ChartThemeDark);
    barChart->setAnimationOptions(QChart::SeriesAnimations);
    barChart->legend()->setAlignment(Qt::AlignBottom);
    chartRevenueView = new QChartView(barChart, chartsGroup);
    chartRevenueView->setRenderHint(QPainter::Antialiasing);
    chartsLayout->addWidget(chartRevenueView);

    // Вставляем в главный layout между counters и top clients
    auto *mainLayout = qobject_cast<QVBoxLayout*>(ui->centralwidget->layout());
    if (mainLayout) {
        int idx = mainLayout->indexOf(ui->groupBoxTopClients);
        if (idx >= 0)
            mainLayout->insertWidget(idx, chartsGroup);
        else
            mainLayout->addWidget(chartsGroup);
    }

    updateCharts();
}

void MainWindow::updateCharts()
{
    QSqlQuery query(DatabaseManager::instance().getDatabase());

    // Pie chart — статусы терминалов
    auto *pieChart = qobject_cast<QChart*>(chartStatusView->chart());
    if (pieChart) {
        pieChart->removeAllSeries();
        auto *pieSeries = new QPieSeries();
        if (query.exec("SELECT CASE status WHEN 0 THEN 'Свободен' WHEN 1 THEN 'В аренде' WHEN 2 THEN 'В ремонте' WHEN 3 THEN 'Списан' WHEN 4 THEN 'Утерян' ELSE 'Прочее' END, COUNT(*) FROM tblterminals GROUP BY status ORDER BY status")) {
            while (query.next())
                pieSeries->append(query.value(0).toString(), query.value(1).toInt());
        }
        pieChart->addSeries(pieSeries);
    }

    // Bar chart — выручка за последние 6 месяцев
    auto *barChart = qobject_cast<QChart*>(chartRevenueView->chart());
    if (barChart) {
        barChart->removeAllSeries();
        // Удаляем старые оси, чтобы не накапливались при каждом обновлении
        const auto oldAxes = barChart->axes();
        for (auto *axis : oldAxes)
            barChart->removeAxis(axis);

        auto *barSet = new QBarSet("Оплаты");
        barSet->setColor("#1976D2");
        QStringList categories;

        if (query.exec(
            "SELECT to_char(periodyear || '-' || LPAD(periodmonth::text, 2, '0'), 'YYYY-MM') AS month, "
            "COALESCE(SUM(amount), 0) AS total "
            "FROM tblpayments "
            "WHERE (periodyear * 12 + periodmonth) >= (EXTRACT(YEAR FROM CURRENT_DATE) * 12 + EXTRACT(MONTH FROM CURRENT_DATE) - 5) "
            "GROUP BY periodyear, periodmonth ORDER BY periodyear, periodmonth")) {
            while (query.next()) {
                categories << query.value(0).toString();
                *barSet << query.value(1).toDouble();
            }
        }

        auto *barSeries = new QBarSeries();
        barSeries->append(barSet);
        barChart->addSeries(barSeries);

        auto *axisX = new QBarCategoryAxis();
        axisX->append(categories);
        barChart->addAxis(axisX, Qt::AlignBottom);
        barSeries->attachAxis(axisX);

        auto *axisY = new QValueAxis();
        axisY->setTitleText("Сумма, руб.");
        barChart->addAxis(axisY, Qt::AlignLeft);
        barSeries->attachAxis(axisY);
    }
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
        updateCounterWidget(ui->labelValueTotal, ui->labelNameTotal,
            query.value(0).toString(), "Всего терминалов", "#3498db");
    }

    if (query.exec("SELECT COUNT(*) FROM tblterminals WHERE status = 0") && query.next()) {
        updateCounterWidget(ui->labelValueFree, ui->labelNameFree,
            query.value(0).toString(), "Свободно терминалов", "#2ecc71");
    }

    if (query.exec("SELECT COUNT(*) FROM tblterminals WHERE status = 1") && query.next()) {
        updateCounterWidget(ui->labelValueRented, ui->labelNameRented,
            query.value(0).toString(), "В аренде", "#e74c3c");
    }

    if (query.exec("SELECT COUNT(*) FROM tblsimcards") && query.next()) {
        updateCounterWidget(ui->labelValueTotalSIM, ui->labelNameTotalSIM,
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
        updateCounterWidget(ui->labelValueFreeSIM, ui->labelNameFreeSIM,
            query.value(0).toString(), "Свободно SIM", "#1abc9c");
    }

    if (query.exec("SELECT COUNT(*) FROM tblclients") && query.next()) {
        updateCounterWidget(ui->labelValueClients, ui->labelNameClients,
            query.value(0).toString(), "Клиентов", "#f39c12");
    }

}

void MainWindow::updateCounterWidget(QLabel* valueLabel, QLabel* nameLabel,
                                     const QString& value, const QString& label,
                                     const QString& color)
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

void MainWindow::openForm(QWidget *form)
{
    form->setAttribute(Qt::WA_DeleteOnClose);
    form->setWindowModality(Qt::WindowModal);
    form->show();
    form->activateWindow();

    // Центрирование относительно главного окна (после show, когда размеры известны)
    QRect mainRect = this->geometry();
    int x = mainRect.x() + (mainRect.width() - form->width()) / 2;
    int y = mainRect.y() + (mainRect.height() - form->height()) / 2;
    form->move(x, y);
}

void MainWindow::loadTopClients()
{
    QString queryStr =
        "SELECT c.clientid, c.clientname AS \"Клиент\", "
        "COUNT(t.terminalid) AS \"Терминалов в аренде\" "
        "FROM tblclients c "
        "JOIN tblrentaldocs r ON c.clientid = r.clientid "
        "JOIN tblrentaldetails rd ON r.rentaldocid = rd.rentaldocid "
        "JOIN tblterminals t ON rd.terminalid = t.terminalid AND t.status = 1 "
        "GROUP BY c.clientid, c.clientname "
        "ORDER BY COUNT(t.terminalid) DESC";

    topClientsModel->setQuery(queryStr, DatabaseManager::instance().getDatabase());
    ui->tableViewTopClients->hideColumn(0);
}

void MainWindow::loadRecentDocuments()
{
    QString queryStr =
        "SELECT 1 AS doctype, receiptdocid AS docid, docnumber AS \"Номер\", docdate AS \"Дата\", 'Поступление' AS \"Тип\" FROM tblreceiptdocs "
        "UNION ALL "
        "SELECT 2, rentaldocid, docnumber, docdate, 'Аренда' FROM tblrentaldocs "
        "UNION ALL "
        "SELECT 3, returndocid, docnumber, docdate, 'Возврат' FROM tblreturndocs "
        "ORDER BY \"Дата\" DESC "
        "LIMIT 15";

    recentDocsModel->setQuery(queryStr, DatabaseManager::instance().getDatabase());
    ui->tableViewRecentDocs->hideColumn(0);
    ui->tableViewRecentDocs->hideColumn(1);
}

void MainWindow::onDatabaseDataChanged()
{
    loadCounters();
    loadTopClients();
    loadRecentDocuments();
    ui->labelLastUpdate->setText("Последнее обновление: " +
        QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss"));
}

void MainWindow::onRecentDocDoubleClicked(const QModelIndex &index)
{
    int docType = recentDocsModel->data(recentDocsModel->index(index.row(), 0)).toInt();
    int docId = recentDocsModel->data(recentDocsModel->index(index.row(), 1)).toInt();
    if (docId <= 0) return;

    if (docType == 1) {
        ReceiptForm form(this);
        form.loadForEdit(docId);
        form.exec();
    } else if (docType == 2) {
        RentalForm form(this);
        form.loadForEdit(docId);
        form.exec();
    } else if (docType == 3) {
        ReturnForm form(this);
        form.loadForEdit(docId);
        form.exec();
    }
    loadRecentDocuments();
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

void MainWindow::onActionArchivePayment_triggered()
{
    openForm(new ArchiveDocumentsForm(4, this));
}

void MainWindow::onActionTerminalHistory_triggered()
{
    QDialog dialog(this);
    dialog.setWindowTitle("История терминала");
    dialog.resize(400, 100);

    auto *layout = new QVBoxLayout(&dialog);
    auto *label = new QLabel("Введите или выберите серийный номер:", &dialog);
    auto *combo = new QComboBox(&dialog);
    combo->setEditable(true);
    combo->setInsertPolicy(QComboBox::NoInsert);

    // Загружаем все серийные номера
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.exec("SELECT terminalid, serialnumber FROM tblterminals ORDER BY serialnumber");

    struct TermInfo { int id; QString serial; };
    QList<TermInfo> terminals;
    while (query.next()) {
        terminals.append({query.value(0).toInt(), query.value(1).toString()});
    }

    for (const auto &t : terminals) {
        combo->addItem(t.serial, t.id);
    }

    // Автокомплит с поиском по вхождению подстроки
    QCompleter *completer = new QCompleter(combo->model(), &dialog);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    combo->setCompleter(completer);

    auto *btnLayout = new QHBoxLayout();
    auto *btnOk = new QPushButton("Открыть", &dialog);
    auto *btnCancel = new QPushButton("Отмена", &dialog);
    btnLayout->addStretch();
    btnLayout->addWidget(btnOk);
    btnLayout->addWidget(btnCancel);

    layout->addWidget(label);
    layout->addWidget(combo);
    layout->addLayout(btnLayout);

    connect(btnOk, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(btnCancel, &QPushButton::clicked, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) return;

    QString serial = combo->currentText().trimmed();
    if (serial.isEmpty()) return;

    // Ищем терминал
    QSqlQuery findQuery(DatabaseManager::instance().getDatabase());
    findQuery.prepare("SELECT terminalid FROM tblterminals WHERE serialnumber = :sn");
    findQuery.bindValue(":sn", serial);

    if (findQuery.exec() && findQuery.next()) {
        int terminalId = findQuery.value(0).toInt();
        openForm(new TerminalHistoryForm(terminalId, serial, this));
    } else {
        QMessageBox::warning(this, "Ошибка",
            QString("Терминал с серийным номером «%1» не найден.").arg(serial));
    }
}

void MainWindow::onActionFreeDevicesReport_triggered()
{
    openFreeDevicesReport();
}

void MainWindow::onActionBulkImport_triggered()
{
    openBulkImport();
}

void MainWindow::onActionBackup_triggered()
{
    performBackup();
}

void MainWindow::onActionExpiryNotifications_triggered()
{
    showExpiryNotifications();
}

void MainWindow::openFreeDevicesReport()
{
    // Создаём модальное окно с отчётом
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Отчёт: Свободные терминалы и SIM-карты");
    dialog->resize(900, 500);

    auto *layout = new QVBoxLayout(dialog);

    // GroupBox для терминалов
    auto *termGroupBox = new QGroupBox("Свободные терминалы", dialog);
    auto *termLayout = new QVBoxLayout(termGroupBox);
    auto *termModel = new QSqlQueryModel(termGroupBox);
    auto *termView = new QTableView(termGroupBox);

    QString termQuery =
        "SELECT t.serialnumber, m.modelname, "
        "COALESCE(s.simnumber, 'SIM не назначена') AS simstatus "
        "FROM tblterminals t "
        "LEFT JOIN tblmodels m ON t.modelid = m.modelid "
        "LEFT JOIN tblsimcards s ON t.currentsimcardid = s.simcardid "
        "WHERE t.status = 0 "
        "ORDER BY t.serialnumber";

    termModel->setQuery(termQuery, DatabaseManager::instance().getDatabase());
    termView->setModel(termModel);
    termView->setSelectionBehavior(QAbstractItemView::SelectRows);
    termView->setSelectionMode(QAbstractItemView::SingleSelection);
    termView->horizontalHeader()->setStretchLastSection(true);
    termView->setAlternatingRowColors(true);
    termView->setColumnWidth(0, 200);
    termView->setColumnWidth(1, 200);
    termView->setColumnWidth(2, 200);
    termLayout->addWidget(termView);

    // GroupBox для SIM-карт
    auto *simGroupBox = new QGroupBox("Свободные SIM-карты", dialog);
    auto *simLayout = new QVBoxLayout(simGroupBox);
    auto *simModel = new QSqlQueryModel(simGroupBox);
    auto *simView = new QTableView(simGroupBox);

    QString simQuery =
        "SELECT s.simnumber, s.notes, "
        "'Не используется' AS status "
        "FROM tblsimcards s "
        "WHERE s.status = 0 "
        "AND s.simcardid NOT IN ("
        "    SELECT t.currentsimcardid FROM tblterminals t WHERE t.currentsimcardid IS NOT NULL"
        ") "
        "ORDER BY s.simnumber";

    simModel->setQuery(simQuery, DatabaseManager::instance().getDatabase());
    simView->setModel(simModel);
    simView->setSelectionBehavior(QAbstractItemView::SelectRows);
    simView->setSelectionMode(QAbstractItemView::SingleSelection);
    simView->horizontalHeader()->setStretchLastSection(true);
    simView->setAlternatingRowColors(true);
    simView->setColumnWidth(0, 250);
    simView->setColumnWidth(1, 300);
    simLayout->addWidget(simView);

    // Кнопки экспорта
    auto *btnLayout = new QHBoxLayout();
    auto *btnExportTerm = new QPushButton("Экспорт терминалов в Excel", dialog);
    auto *btnExportSim = new QPushButton("Экспорт SIM-карт в Excel", dialog);
    auto *btnClose = new QPushButton("Закрыть", dialog);

    connect(btnExportTerm, &QPushButton::clicked, [termView, dialog]() {
        auto *model = qobject_cast<QSqlQueryModel*>(termView->model());
        if (!model) return;
        QString filePath = QFileDialog::getSaveFileName(dialog,
            "Экспорт свободных терминалов", "free_terminals.xlsx", "Excel (*.xlsx);;Все файлы (*)");
        if (!filePath.isEmpty()) {
            if (ReportExporter::exportModelToExcel(model, "Свободные терминалы", filePath)) {
                QMessageBox::information(dialog, "Успех", "Терминалы экспортированы.");
            }
        }
    });

    connect(btnExportSim, &QPushButton::clicked, [simView, dialog]() {
        auto *model = qobject_cast<QSqlQueryModel*>(simView->model());
        if (!model) return;
        QString filePath = QFileDialog::getSaveFileName(dialog,
            "Экспорт свободных SIM", "free_simcards.xlsx", "Excel (*.xlsx);;Все файлы (*)");
        if (!filePath.isEmpty()) {
            if (ReportExporter::exportModelToExcel(model, "Свободные SIM-карты", filePath)) {
                QMessageBox::information(dialog, "Успех", "SIM-карты экспортированы.");
            }
        }
    });

    btnLayout->addWidget(btnExportTerm);
    btnLayout->addWidget(btnExportSim);
    btnLayout->addStretch();
    btnLayout->addWidget(btnClose);

    layout->addWidget(termGroupBox);
    layout->addWidget(simGroupBox);
    layout->addLayout(btnLayout);

    connect(btnClose, &QPushButton::clicked, dialog, &QDialog::accept);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->open();
}

void MainWindow::onTopClientDoubleClicked(const QModelIndex &index)
{
    int clientId = topClientsModel->data(topClientsModel->index(index.row(), 0)).toInt();
    QString clientName = topClientsModel->data(topClientsModel->index(index.row(), 1)).toString();
    if (clientId <= 0) return;
    openClientRentalReport(clientId, clientName);
}

void MainWindow::openClientRentalReport(int clientId, const QString &clientName)
{
    QString title = QString("Клиент: %1 — Терминалы в аренде").arg(clientName);

    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle(title);
    dialog->resize(900, 500);
    dialog->setStyleSheet(
        "QDialog { background-color: #1E1E1E; }"
        "QLabel#headerLabel { font-size: 18px; font-weight: bold; color: #FFFFFF; padding: 12px; }"
    );

    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto *headerLabel = new QLabel(title, dialog);
    headerLabel->setObjectName("headerLabel");
    layout->addWidget(headerLabel);

    auto *groupBox = new QGroupBox("Арендованные терминалы", dialog);
    groupBox->setStyleSheet(
        "QGroupBox { font-size: 13px; font-weight: bold; color: #CCCCCC; "
        "border: 1px solid #3C3C3C; border-radius: 6px; margin-top: 8px; padding-top: 16px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 6px; }"
    );
    auto *groupLayout = new QVBoxLayout(groupBox);

    auto *model = new QSqlQueryModel(groupBox);
    auto *tableView = new QTableView(groupBox);
    tableView->setModel(model);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView->setAlternatingRowColors(true);
    tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableView->verticalHeader()->hide();
    tableView->setStyleSheet(
        "QTableView { background-color: #252526; alternate-background-color: #2A2A2A; "
        "color: #E0E0E0; gridline-color: #333333; border: 1px solid #3C3C3C; border-radius: 4px; "
        "selection-background-color: #1565C0; selection-color: white; }"
        "QHeaderView::section { background: #2D2D2D; color: #FFFFFF; padding: 8px; "
        "border: none; border-bottom: 2px solid #0D47A1; font-weight: bold; font-size: 12px; }"
    );

    QString queryStr =
        "SELECT m.modelname AS \"Модель\", "
        "t.serialnumber AS \"Серийный номер\", "
        "COALESCE(s.simnumber, '—') AS \"SIM-карта\", "
        "r.docdate::date AS \"Дата передачи\" "
        "FROM tblrentaldocs r "
        "JOIN tblrentaldetails rd ON r.rentaldocid = rd.rentaldocid "
        "JOIN tblterminals t ON rd.terminalid = t.terminalid AND t.status = 1 "
        "LEFT JOIN tblmodels m ON t.modelid = m.modelid "
        "LEFT JOIN tblsimcards s ON rd.simcardid = s.simcardid "
        "WHERE r.clientid = :clientId "
        "ORDER BY r.docdate DESC, t.serialnumber";

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare(queryStr);
    query.bindValue(":clientId", clientId);
    query.exec();
    model->setQuery(std::move(query));

    groupLayout->addWidget(tableView);
    layout->addWidget(groupBox);

    // Кнопки
    auto *btnLayout = new QHBoxLayout();
    auto *btnExport = new QPushButton("Экспорт в Excel", dialog);
    btnExport->setStyleSheet(
        "QPushButton { background-color: #1565C0; color: white; padding: 8px 20px; "
        "border: none; border-radius: 4px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background-color: #1976D2; }"
    );
    auto *btnClose = new QPushButton("Закрыть", dialog);
    btnClose->setStyleSheet(
        "QPushButton { background-color: #424242; color: white; padding: 8px 20px; "
        "border: none; border-radius: 4px; font-size: 13px; }"
        "QPushButton:hover { background-color: #616161; }"
    );

    connect(btnExport, &QPushButton::clicked, [model, clientName, dialog]() {
        QString filePath = QFileDialog::getSaveFileName(dialog,
            "Экспорт отчёта",
            QString("terminals_%1.xlsx").arg(clientName.simplified().replace(' ', '_')),
            "Excel (*.xlsx);;Все файлы (*)");
        if (!filePath.isEmpty()) {
            if (ReportExporter::exportModelToExcel(model, clientName, filePath)) {
                QMessageBox::information(dialog, "Успех", "Отчёт экспортирован.");
            }
        }
    });
    connect(btnClose, &QPushButton::clicked, dialog, &QDialog::accept);

    btnLayout->addWidget(btnExport);
    btnLayout->addStretch();
    btnLayout->addWidget(btnClose);
    layout->addLayout(btnLayout);

    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->open();
}

void MainWindow::onActionAuditLog_triggered()
{
    openForm(new AuditLogForm(this));
}

void MainWindow::onActionBatchStatus_triggered()
{
    openForm(new BatchStatusForm(this));
}

void MainWindow::onActionReports_triggered()
{
    openForm(new ReportsForm(this));
}

void MainWindow::openBulkImport()
{
    openForm(new BulkImportForm(this));
}

void MainWindow::performBackup()
{
    QString filePath = QFileDialog::getSaveFileName(this,
        "Сохранить резервную копию БД",
        QString("backup_poc_%1.sql").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")),
        "SQL файлы (*.sql);;Все файлы (*)");

    if (filePath.isEmpty()) return;

    QMessageBox::StandardButton reply = QMessageBox::question(this, "Подтверждение",
        "Для выполнения резервного копирования необходимо:\n"
        "1. Утилита pg_dump должна быть доступна в PATH\n"
        "2. Или указать путь к pg_dump вручную\n\n"
        "Выполнить резервное копирование?", QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    // Загружаем конфигурацию БД
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.exec("SELECT current_setting('port'), current_database(), current_user");
    if (!query.next()) return;

    QString port = query.value(0).toString();
    QString dbname = query.value(1).toString();
    QString user = query.value(2).toString();

    // Показываем диалог для ввода пароля
    bool passwordOk;
    QString password = QInputDialog::getText(this, "Пароль PostgreSQL",
        "Введите пароль для пользователя " + user + ":", QLineEdit::Password, QString(), &passwordOk);
    if (!passwordOk) return;

    // Создаём временный .pgpass для безопасной аутентификации
    QTemporaryFile pgpassFile(QDir::tempPath() + "/pgpass_XXXXXX");
    pgpassFile.setAutoRemove(true);
    if (!pgpassFile.open()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось создать временный файл для аутентификации.");
        return;
    }
    QString pgpassPath = pgpassFile.fileName();
    pgpassFile.write(QString("localhost:%1:%2:%3:%4")
                     .arg(port, dbname, user, password).toUtf8());
    pgpassFile.close();

    // Формируем команду pg_dump
    QStringList args;
    args << "--format=plain"
         << "--encoding=UTF8"
         << "--no-password"
         << "--host=localhost"
         << QString("--port=%1").arg(port)
         << QString("--username=%1").arg(user)
         << QString("--file=%1").arg(filePath)
         << dbname;

    QProcess process;
    auto env = process.environment();
    env.append(QString("PGPASSFILE=%1").arg(pgpassPath));
    process.setEnvironment(env);
    process.start("pg_dump", args);

    if (!process.waitForFinished(60000)) {
        QMessageBox::warning(this, "Ошибка резервного копирования",
            "Не удалось выполнить pg_dump.\n"
            "Убедитесь, что PostgreSQL клиент установлен и доступен в PATH.\n\n"
            "Альтернативный метод (SQL-экспорт через Qt SQL):\n"
            "Резервная копия будет создана через SQL-запросы.");

        // Фоллбэк: экспорт всех данных через SQL
        performFallbackBackup(filePath, dbname, password);
    } else {
        QString output = process.readAllStandardOutput();
        QString error = process.readAllStandardError();

        if (!error.isEmpty() && !error.contains("Enter password")) {
            QMessageBox::warning(this, "Ошибка резервного копирования",
                "pg_dump вернул ошибки:\n" + error);
        } else {
            QMessageBox::information(this, "Успех",
                QString("Резервная копия создана:\n%1")
                .arg(filePath) +
                QString("\nРазмер: %1 KB")
                .arg(QFileInfo(filePath).size() / 1024));
        }
    }
}

void MainWindow::performFallbackBackup(const QString &filePath, const QString &dbname, const QString &/*password*/)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Ошибка", "Не удалось создать файл резервной копии.");
        return;
    }

    QTextStream out(&file);
    QSqlDatabase db = DatabaseManager::instance().getDatabase();

    out << "-- Резервная копия БД " << dbname << "\n";
    out << "-- Создана: " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n\n";

    QSqlQuery tableQuery(db);
    tableQuery.exec("SELECT tablename FROM pg_tables WHERE schemaname = 'public' ORDER BY tablename");

    while (tableQuery.next()) {
        QString table = tableQuery.value(0).toString();

        out << "-- Таблица: " << table << "\n";
        out << "CREATE TABLE IF NOT EXISTS \"" << table << "\" (\n";

        QSqlQuery colQuery(db);
        colQuery.prepare("SELECT column_name, data_type, is_nullable "
                         "FROM information_schema.columns "
                         "WHERE table_schema = 'public' AND table_name = :tbl "
                         "ORDER BY ordinal_position");
        colQuery.bindValue(":tbl", table);
        colQuery.exec();

        QStringList columnDefs;
        QStringList columnNames;
        while (colQuery.next()) {
            QString name = colQuery.value(0).toString();
            QString type = colQuery.value(1).toString();
            QString nullable = colQuery.value(2).toString();
            columnNames.append("\"" + name + "\"");

            if (type.toUpper().startsWith("INT")) type = "INTEGER";
            columnDefs.append("    \"" + name + "\" " + type +
                              (nullable == "YES" ? " NULL" : " NOT NULL"));
        }
        out << columnDefs.join(",\n") << "\n);\n\n";

        QSqlQuery dataQuery(db);
        dataQuery.prepare(QString("SELECT * FROM \"%1\"").arg(table));
        dataQuery.exec();

        while (dataQuery.next()) {
            out << "INSERT INTO \"" << table << "\" (" << columnNames.join(", ") << ") VALUES (";
            for (int i = 0; i < dataQuery.record().count(); i++) {
                if (i > 0) out << ", ";
                QVariant val = dataQuery.value(i);
                if (val.isNull()) {
                    out << "NULL";
                } else {
                    QMetaType::Type t = static_cast<QMetaType::Type>(val.typeId());
                    if (t == QMetaType::QString || t == QMetaType::QByteArray) {
                        QString escaped = val.toString();
                        escaped.replace("\\", "\\\\");
                        escaped.replace("'", "''");
                        out << "'" << escaped << "'";
                    } else if (t == QMetaType::QDateTime || t == QMetaType::QDate) {
                        out << "'" << val.toString() << "'";
                    } else if (t == QMetaType::QByteArray) {
                        out << "'\\x" << val.toByteArray().toHex() << "'";
                    } else {
                        out << val.toString();
                    }
                }
            }
            out << ");\n";
        }
        out << "\n";
    }

    file.close();
    QMessageBox::information(this, "Успех",
        QString("Резервная копия создана (SQL-метод):\n%1\nРазмер: %2 KB")
        .arg(filePath)
        .arg(QFileInfo(filePath).size() / 1024));
}

void MainWindow::showExpiryNotifications()
{
    openForm(new ExpiryNotificationsForm(this));
}

void MainWindow::onActionUserManagement_triggered()
{
    openForm(new UserManagementForm(this));
}

void MainWindow::showGlobalSearch()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Глобальный поиск (Ctrl+K)");
    dialog.resize(550, 400);
    dialog.setStyleSheet("QDialog { background-color: #252526; }");

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto *input = new QLineEdit(&dialog);
    input->setPlaceholderText("Введите запрос (серийник, IMEI, клиент, SIM, модель...)");
    input->setClearButtonEnabled(true);
    layout->addWidget(input);

    auto *list = new QListWidget(&dialog);
    list->setAlternatingRowColors(true);
    layout->addWidget(list);

    auto *btnLayout = new QHBoxLayout();
    auto *btnOpen = new QPushButton("Открыть", &dialog);
    auto *btnCancel = new QPushButton("Отмена", &dialog);
    btnLayout->addStretch();
    btnLayout->addWidget(btnOpen);
    btnLayout->addWidget(btnCancel);
    layout->addLayout(btnLayout);

    connect(btnCancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(btnOpen, &QPushButton::clicked, &dialog, &QDialog::accept);

    // Поиск по мере ввода
    connect(input, &QLineEdit::textChanged, &dialog, [list, input]() {
        list->clear();
        QString q = input->text().trimmed();
        if (q.length() < 2) return;

        list->addItem("Поиск...");

        struct SearchResult {
            int type; // 1=terminal, 2=client, 3=sim, 4=model, 5=manufacturer
            int id;
            QString text;
        };
        QList<SearchResult> results;
        QSqlDatabase db = DatabaseManager::instance().getDatabase();
        QString like = "%" + q.replace("\\", "\\\\").replace("'", "''").replace("%", "\\%").replace("_", "\\_") + "%";

        QSqlQuery query(db);
        query.prepare("SELECT terminalid, serialnumber, COALESCE(imei1,''), COALESCE(imei2,'') "
                      "FROM tblterminals WHERE serialnumber ILIKE :q "
                      "OR imei1 ILIKE :q2 OR imei2 ILIKE :q3 LIMIT 15");
        query.bindValue(":q", like);
        query.bindValue(":q2", like);
        query.bindValue(":q3", like);
        if (query.exec()) {
            while (query.next())
                results.append({1, query.value(0).toInt(), query.value(1).toString() + " (Терминал)"});
        }

        query.prepare("SELECT clientid, clientname FROM tblclients WHERE clientname ILIKE :q OR inn ILIKE :q2 LIMIT 10");
        query.bindValue(":q", like);
        query.bindValue(":q2", like);
        if (query.exec()) {
            while (query.next())
                results.append({2, query.value(0).toInt(), query.value(1).toString() + " (Клиент)"});
        }

        query.prepare("SELECT simcardid, simnumber FROM tblsimcards WHERE simnumber ILIKE :q LIMIT 10");
        query.bindValue(":q", like);
        if (query.exec()) {
            while (query.next())
                results.append({3, query.value(0).toInt(), query.value(1).toString() + " (SIM)"});
        }

        query.prepare("SELECT modelid, modelname FROM tblmodels WHERE modelname ILIKE :q LIMIT 10");
        query.bindValue(":q", like);
        if (query.exec()) {
            while (query.next())
                results.append({4, query.value(0).toInt(), query.value(1).toString() + " (Модель)"});
        }

        query.prepare("SELECT manufacturerid, manufacturername FROM tblmanufacturers WHERE manufacturername ILIKE :q LIMIT 5");
        query.bindValue(":q", like);
        if (query.exec()) {
            while (query.next())
                results.append({5, query.value(0).toInt(), query.value(1).toString() + " (Производитель)"});
        }

        list->clear();
        for (const auto &r : results) {
            auto *item = new QListWidgetItem(r.text);
            item->setData(Qt::UserRole, r.type);
            item->setData(Qt::UserRole + 1, r.id);
            list->addItem(item);
        }
        if (results.isEmpty())
            list->addItem("Ничего не найдено");
    });

    // Двойной клик = открыть
    connect(list, &QListWidget::itemDoubleClicked, &dialog, &QDialog::accept);

    if (dialog.exec() != QDialog::Accepted) return;

    auto *item = list->currentItem();
    if (!item || item->data(Qt::UserRole).isNull()) return;

    int type = item->data(Qt::UserRole).toInt();
    int id = item->data(Qt::UserRole + 1).toInt();

    switch (type) {
    case 1:
        openForm(new TerminalsForm(this));
        break;
    case 2:
        openForm(new ClientsForm(this));
        break;
    case 3:
        openForm(new SIMCardsForm(this));
        break;
    case 4:
        openForm(new ModelsForm(this));
        break;
    case 5:
        openForm(new ManufacturersForm(this));
        break;
    }
}