#include "auditlogform.h"
#include "ui_auditlogform.h"
#include "database/databasemanager.h"
#include "utils/reportexporter.h"
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDate>
#include <QFileDialog>

AuditLogForm::AuditLogForm(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AuditLogForm),
    model(new QSqlQueryModel(this))
{
    ui->setupUi(this);
    setWindowTitle("Журнал аудита");
    resize(1000, 600);

    QDate today = QDate::currentDate();
    ui->dateEditFrom->setDate(QDate(today.year(), today.month(), 1));
    ui->dateEditTo->setDate(today);

    loadFilterValues();

    ui->tableView->setModel(model);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setAlternatingRowColors(true);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    ui->tableView->setColumnWidth(0, 80);
    ui->tableView->setColumnWidth(1, 100);
    ui->tableView->setColumnWidth(2, 120);
    ui->tableView->setColumnWidth(3, 150);
    ui->tableView->setColumnWidth(4, 80);

    applyFilter();
}

AuditLogForm::~AuditLogForm()
{
    delete ui;
}

void AuditLogForm::loadFilterValues()
{
    ui->comboBoxAction->clear();
    ui->comboBoxAction->addItem("Все действия", "");
    ui->comboBoxAction->addItem("POST (Проведение)", "POST");
    ui->comboBoxAction->addItem("ADD (Добавление)", "ADD");
    ui->comboBoxAction->addItem("UPDATE (Изменение)", "UPDATE");
    ui->comboBoxAction->addItem("DELETE (Удаление)", "DELETE");

    ui->comboBoxTable->clear();
    ui->comboBoxTable->addItem("Все таблицы", "");
    ui->comboBoxTable->addItem("tblreceiptdocs", "tblreceiptdocs");
    ui->comboBoxTable->addItem("tblrentaldocs", "tblrentaldocs");
    ui->comboBoxTable->addItem("tblreturndocs", "tblreturndocs");
    ui->comboBoxTable->addItem("tblpayments", "tblpayments");
    ui->comboBoxTable->addItem("tblterminals", "tblterminals");
    ui->comboBoxTable->addItem("tblsimcards", "tblsimcards");
    ui->comboBoxTable->addItem("tblclients", "tblclients");
    ui->comboBoxTable->addItem("tblmodels", "tblmodels");
    ui->comboBoxTable->addItem("tblmanufacturers", "tblmanufacturers");
}

void AuditLogForm::applyFilter()
{
    QString dateFrom = ui->dateEditFrom->date().toString("yyyy-MM-dd");
    QString dateTo = ui->dateEditTo->date().toString("yyyy-MM-dd") + " 23:59:59";
    QString action = ui->comboBoxAction->currentData().toString();
    QString tableName = ui->comboBoxTable->currentData().toString();

    QString queryStr =
        "SELECT audit_log_id AS \"ID\", "
        "username AS \"Пользователь\", "
        "action AS \"Действие\", "
        "table_name AS \"Таблица\", "
        "record_id AS \"Запись\", "
        "performed_at AS \"Дата/время\" "
        "FROM tbl_audit_log "
        "WHERE performed_at BETWEEN :dateFrom AND :dateTo";

    if (!action.isEmpty()) {
        queryStr += " AND action = :action";
    }
    if (!tableName.isEmpty()) {
        queryStr += " AND table_name = :table";
    }

    queryStr += " ORDER BY performed_at DESC LIMIT 500";

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare(queryStr);
    query.bindValue(":dateFrom", dateFrom);
    query.bindValue(":dateTo", dateTo);
    if (!action.isEmpty()) {
        query.bindValue(":action", action);
    }
    if (!tableName.isEmpty()) {
        query.bindValue(":table", tableName);
    }

    if (!query.exec()) {
        QMessageBox::critical(this, "Ошибка", query.lastError().text());
        return;
    }

    model->setQuery(std::move(query));
    model->setHeaderData(0, Qt::Horizontal, "ID");
    model->setHeaderData(1, Qt::Horizontal, "Пользователь");
    model->setHeaderData(2, Qt::Horizontal, "Действие");
    model->setHeaderData(3, Qt::Horizontal, "Таблица");
    model->setHeaderData(4, Qt::Horizontal, "Запись");
    model->setHeaderData(5, Qt::Horizontal, "Дата/время");

    ui->tableView->resizeColumnsToContents();
}

void AuditLogForm::on_btnFilter_clicked()
{
    applyFilter();
}

void AuditLogForm::on_btnExportExcel_clicked()
{
    QString filePath = ReportExporter::getSaveFilePath(
        this, "Сохранить журнал аудита в Excel",
        "Excel файлы (*.xlsx)");

    if (filePath.isEmpty()) return;

    ReportExporter::exportModelToExcel(model, "Журнал аудита", filePath, this);
}

void AuditLogForm::on_btnClose_clicked()
{
    close();
}
