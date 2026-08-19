#include "reportsform.h"
#include "ui_reportsform.h"
#include "database/databasemanager.h"
#include "utils/reportexporter.h"
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDate>
#include <QFileDialog>

ReportsForm::ReportsForm(QWidget* parent) : QDialog(parent), ui(new Ui::ReportsForm), model(new QSqlQueryModel(this))
{
    ui->setupUi(this);
    setWindowTitle("Отчёты по периодам");
    resize(1000, 650);

    QDate today = QDate::currentDate();
    ui->dateEditFrom->setDate(QDate(today.year(), today.month(), 1));
    ui->dateEditTo->setDate(today);

    loadReportTypes();

    ui->tableView->setModel(model);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setAlternatingRowColors(true);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

ReportsForm::~ReportsForm()
{
    delete ui;
}

void ReportsForm::loadReportTypes()
{
    ui->comboBoxReport->clear();
    ui->comboBoxReport->addItem("Выручка по клиентам", "revenue");
    ui->comboBoxReport->addItem("Загрузка терминалов", "load");
    ui->comboBoxReport->addItem("Конвертация аренды в оплату", "conversion");
    ui->comboBoxReport->addItem("Использование SIM-карт", "sim_usage");
    ui->comboBoxReport->addItem("Задолженность клиентов", "debt");
}

void ReportsForm::on_btnGenerate_clicked()
{
    QString reportType = ui->comboBoxReport->currentData().toString();

    if (reportType == "revenue")
        generateRevenueByClient();
    else if (reportType == "load")
        generateTerminalLoad();
    else if (reportType == "conversion")
        generateRentalConversion();
    else if (reportType == "sim_usage")
        generateSIMUsage();
    else if (reportType == "debt")
        generateDebtReport();
}

void ReportsForm::generateRevenueByClient()
{
    QString dateFrom = ui->dateEditFrom->date().toString("yyyy-MM-dd");
    QString dateTo = ui->dateEditTo->date().toString("yyyy-MM-dd");

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("SELECT c.clientname AS \"Клиент\", "
                  "COUNT(DISTINCT p.paymentid) AS \"Платежей\", "
                  "COALESCE(SUM(p.amount), 0) AS \"Сумма оплат\", "
                  "COUNT(DISTINCT rd.terminalid) AS \"Терминалов в аренде\" "
                  "FROM tblclients c "
                  "LEFT JOIN tblpayments p ON c.clientid = p.clientid "
                  "AND p.paymentdate >= :dateFrom::date AND p.paymentdate < :dateTo::date + interval '1 day' "
                  "LEFT JOIN tblrentaldocs r ON c.clientid = r.clientid "
                  "LEFT JOIN tblrentaldetails rd ON r.rentaldocid = rd.rentaldocid "
                  "GROUP BY c.clientid, c.clientname "
                  "ORDER BY COALESCE(SUM(p.amount), 0) DESC");
    query.bindValue(":dateFrom", dateFrom);
    query.bindValue(":dateTo", dateTo);

    if (!query.exec()) {
        QMessageBox::critical(this, "Ошибка", query.lastError().text());
        return;
    }

    model->setQuery(std::move(query));
    ui->tableView->resizeColumnsToContents();

    double total = 0;
    for (int i = 0; i < model->rowCount(); ++i) {
        total += model->data(model->index(i, 2)).toDouble();
    }
    generateSummary(QString("Итого оплат за период: %1 руб.").arg(total, 0, 'f', 2));
}

void ReportsForm::generateTerminalLoad()
{
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare(
        "SELECT m.modelname AS \"Модель\", "
        "COUNT(t.terminalid) AS \"Всего\", "
        "COUNT(CASE WHEN t.status = 0 THEN 1 END) AS \"Свободно\", "
        "COUNT(CASE WHEN t.status = 1 THEN 1 END) AS \"В аренде\", "
        "ROUND(100.0 * COUNT(CASE WHEN t.status = 1 THEN 1 END) / NULLIF(COUNT(t.terminalid), 0), 1) AS \"Загрузка %\" "
        "FROM tblterminals t "
        "LEFT JOIN tblmodels m ON t.modelid = m.modelid "
        "GROUP BY m.modelname "
        "ORDER BY COUNT(t.terminalid) DESC");

    if (!query.exec()) {
        QMessageBox::critical(this, "Ошибка", query.lastError().text());
        return;
    }

    model->setQuery(std::move(query));
    ui->tableView->resizeColumnsToContents();
    generateSummary("Отчёт по загрузке терминалов по моделям");
}

void ReportsForm::generateRentalConversion()
{
    QString dateFrom = ui->dateEditFrom->date().toString("yyyy-MM-dd");
    QString dateTo = ui->dateEditTo->date().toString("yyyy-MM-dd");

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("SELECT c.clientname AS \"Клиент\", "
                  "COUNT(DISTINCT r.rentaldocid) AS \"Документов аренды\", "
                  "COUNT(DISTINCT CASE WHEN pl.linkid IS NOT NULL THEN r.rentaldocid END) AS \"Оплачено\", "
                  "COUNT(DISTINCT CASE WHEN pl.linkid IS NULL THEN r.rentaldocid END) AS \"Не оплачено\", "
                  "ROUND(100.0 * COUNT(DISTINCT CASE WHEN pl.linkid IS NOT NULL THEN r.rentaldocid END) / "
                  "NULLIF(COUNT(DISTINCT r.rentaldocid), 0), 1) AS \"Конвертация %\" "
                  "FROM tblclients c "
                  "JOIN tblrentaldocs r ON c.clientid = r.clientid "
                  "AND r.docdate >= :dateFrom::date AND r.docdate < :dateTo::date + interval '1 day' "
                  "LEFT JOIN tblpayment_rental_links pl ON r.rentaldocid = pl.rentaldocid "
                  "GROUP BY c.clientid, c.clientname "
                  "ORDER BY COUNT(DISTINCT r.rentaldocid) DESC");
    query.bindValue(":dateFrom", dateFrom);
    query.bindValue(":dateTo", dateTo);

    if (!query.exec()) {
        QMessageBox::critical(this, "Ошибка", query.lastError().text());
        return;
    }

    model->setQuery(std::move(query));
    ui->tableView->resizeColumnsToContents();
    generateSummary("Конвертация аренды в оплату по клиентам");
}

void ReportsForm::generateSIMUsage()
{
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare(
        "SELECT "
        "COUNT(*) AS \"Всего SIM\", "
        "COUNT(CASE WHEN s.status = 0 THEN 1 END) AS \"Свободно\", "
        "COUNT(CASE WHEN s.status = 1 THEN 1 END) AS \"В использовании\", "
        "ROUND(100.0 * COUNT(CASE WHEN s.status = 1 THEN 1 END) / NULLIF(COUNT(*), 0), 1) AS \"Использование %\" "
        "FROM tblsimcards s");

    if (!query.exec()) {
        QMessageBox::critical(this, "Ошибка", query.lastError().text());
        return;
    }

    model->setQuery(std::move(query));
    ui->tableView->resizeColumnsToContents();
    generateSummary("Общая статистика использования SIM-карт");
}

void ReportsForm::generateDebtReport()
{
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("SELECT c.clientname AS \"Клиент\", "
                  "COUNT(DISTINCT r.rentaldocid) AS \"Договоров аренды\", "
                  "COUNT(DISTINCT CASE WHEN pl.linkid IS NULL THEN r.rentaldocid END) AS \"Не оплачено\", "
                  "MIN(CASE WHEN pl.linkid IS NULL THEN r.docdate END)::date AS \"Старейший долг\", "
                  "MAX(r.docdate)::date AS \"Последняя аренда\" "
                  "FROM tblclients c "
                  "JOIN tblrentaldocs r ON c.clientid = r.clientid "
                  "LEFT JOIN tblpayment_rental_links pl ON r.rentaldocid = pl.rentaldocid "
                  "GROUP BY c.clientid, c.clientname "
                  "HAVING COUNT(DISTINCT CASE WHEN pl.linkid IS NULL THEN r.rentaldocid END) > 0 "
                  "ORDER BY COUNT(DISTINCT CASE WHEN pl.linkid IS NULL THEN r.rentaldocid END) DESC");

    if (!query.exec()) {
        QMessageBox::critical(this, "Ошибка", query.lastError().text());
        return;
    }

    model->setQuery(std::move(query));
    ui->tableView->resizeColumnsToContents();
    generateSummary("Клиенты с неоплаченными договорами аренды");
}

void ReportsForm::generateSummary(const QString& text)
{
    ui->labelSummary->setText(text);
}

void ReportsForm::on_btnExport_clicked()
{
    QString reportTitle = ui->comboBoxReport->currentText();
    QString filePath = ReportExporter::getSaveFilePath(this, "Сохранить отчёт в Excel", "Excel файлы (*.xlsx)");

    if (filePath.isEmpty())
        return;

    if (!ReportExporter::exportModelToExcel(model, reportTitle, filePath, this)) {
        QMessageBox::critical(this, "Ошибка экспорта", "Не удалось сохранить отчёт в файл:\n" + filePath);
    }
}

void ReportsForm::on_btnClose_clicked()
{
    close();
}
