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
#include <QSqlRecord>
#include <QPushButton>

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
    connect(ui->actionArchivePayment, &QAction::triggered, this, &MainWindow::onActionArchivePayment_triggered);
    connect(ui->actionTerminalHistory, &QAction::triggered, this, &MainWindow::onActionTerminalHistory_triggered);
    connect(ui->actionFreeDevicesReport, &QAction::triggered, this, &MainWindow::onActionFreeDevicesReport_triggered);
    connect(ui->actionBulkImport, &QAction::triggered, this, &MainWindow::onActionBulkImport_triggered);
    connect(ui->actionBackup, &QAction::triggered, this, &MainWindow::onActionBackup_triggered);
    connect(ui->actionExpiryNotifications, &QAction::triggered, this, &MainWindow::onActionExpiryNotifications_triggered);
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

void MainWindow::onActionArchivePayment_triggered()
{
    openForm(new ArchiveDocumentsForm(4, this));
}

void MainWindow::onActionTerminalHistory_triggered()
{
    // Получаем выбранный терминал из справочника
    bool ok;
    QString serial = QInputDialog::getText(this, "История терминала",
        "Введите серийный номер терминала:", QLineEdit::Normal, QString(), &ok);
    if (!ok || serial.isEmpty()) return;

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("SELECT terminalid FROM tblterminals WHERE serialnumber = :sn");
    query.bindValue(":sn", serial);

    if (query.exec() && query.next()) {
        int terminalId = query.value(0).toInt();
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

    connect(btnExportTerm, &QPushButton::clicked, [termModel, dialog]() {
        QString filePath = QFileDialog::getSaveFileName(dialog,
            "Экспорт свободных терминалов", "free_terminals.xlsx", "Excel (*.xlsx);;Все файлы (*)");
        if (!filePath.isEmpty()) {
            if (ReportExporter::exportModelToExcel(termModel, "Свободные терминалы", filePath)) {
                QMessageBox::information(dialog, "Успех", "Терминалы экспортированы.");
            }
        }
    });

    connect(btnExportSim, &QPushButton::clicked, [simModel, dialog]() {
        QString filePath = QFileDialog::getSaveFileName(dialog,
            "Экспорт свободных SIM", "free_simcards.xlsx", "Excel (*.xlsx);;Все файлы (*)");
        if (!filePath.isEmpty()) {
            if (ReportExporter::exportModelToExcel(simModel, "Свободные SIM-карты", filePath)) {
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

    // Устанавливаем пароль в PGPASSWORD
    QProcess process;
    auto env = process.environment();
    for (int i = 0; i < env.size(); ++i) {
        if (env[i].startsWith("PGPASSWORD=")) {
            env[i] = "PGPASSWORD=" + password;
            break;
        }
    }
    env.append("PGPASSWORD=" + password);
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
        out << "CREATE TABLE IF NOT EXISTS " << table << " (\n";

        // Получаем типы столбцов из information_schema
        QSqlQuery colQuery(db);
        colQuery.exec(QString("SELECT column_name, data_type, is_nullable "
                              "FROM information_schema.columns "
                              "WHERE table_schema = 'public' AND table_name = '%1' "
                              "ORDER BY ordinal_position").arg(table));

        QStringList columnDefs;
        while (colQuery.next()) {
            QString name = colQuery.value(0).toString();
            QString type = colQuery.value(1).toString();
            QString nullable = colQuery.value(2).toString();

            if (type.toUpper().startsWith("INT")) type = "INTEGER";
            columnDefs.append("    " + name + " " + type +
                              (nullable == "YES" ? " NULL" : " NOT NULL"));
        }
        out << columnDefs.join("\n") << "\n);\n\n";

        QSqlQuery dataQuery(db);
        dataQuery.exec(QString("SELECT * FROM %1").arg(table));

        while (dataQuery.next()) {
            out << "INSERT INTO " << table << " VALUES (";
            for (int i = 0; i < dataQuery.record().count(); i++) {
                if (i > 0) out << ", ";
                QVariant val = dataQuery.value(i);
                if (val.isNull()) {
                    out << "NULL";
                } else {
                    out << "'" << val.toString().replace("'", "''") << "'";
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
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Уведомления о просрочке");
    dialog->resize(800, 500);

    auto *layout = new QVBoxLayout(dialog);

    auto *lblInfo = new QLabel(
        "Функция уведомлений о просроченной аренде и неоплаченных периодах.\n\n"
        "Для использования данного функционала необходимо:\n"
        "1. Добавить поле «срок аренды» в tblrentaldocs\n"
        "2. Добавить поле «оплачено» в tblpaymentdocs\n\n"
        "На данный момент активных уведомлений нет.", dialog);
    lblInfo->setWordWrap(true);
    layout->addWidget(lblInfo);

    // Добавляем текущую статистику
    QSqlQuery statQuery(DatabaseManager::instance().getDatabase());
    statQuery.exec("SELECT COUNT(*) FROM tblrentaldocs");
    int totalRentals = 0;
    if (statQuery.next()) totalRentals = statQuery.value(0).toInt();

    statQuery.exec("SELECT COUNT(*) FROM tblpaymentdocs");
    int totalPayments = 0;
    if (statQuery.next()) totalPayments = statQuery.value(0).toInt();

    QString statText = QString("Всего арендных документов: %1\n"
                                "Всего документов оплаты: %2")
                           .arg(totalRentals).arg(totalPayments);
    auto *statLbl = new QLabel(statText, dialog);
    statLbl->setStyleSheet("QLabel { color: #666; font-size: 12px; }");
    layout->addWidget(statLbl);

    auto *btnClose = new QPushButton("Закрыть", dialog);
    connect(btnClose, &QPushButton::clicked, dialog, &QDialog::accept);
    layout->addWidget(btnClose);

    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->open();
}