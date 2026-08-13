#include "expirynotificationsform.h"
#include "ui_expirynotificationsform.h"
#include "database/databasemanager.h"
#include "utils/reportexporter.h"
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>

ExpiryNotificationsForm::ExpiryNotificationsForm(QWidget* parent) :
    QDialog(parent), ui(new Ui::ExpiryNotificationsForm), overdueModel(new QSqlQueryModel(this)),
    unpaidModel(new QSqlQueryModel(this))
{
    ui->setupUi(this);
    setWindowTitle("Уведомления о просрочке");
    resize(900, 600);

    ui->tableViewOverdue->setModel(overdueModel);
    ui->tableViewOverdue->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableViewOverdue->setAlternatingRowColors(true);
    ui->tableViewOverdue->horizontalHeader()->setStretchLastSection(true);
    ui->tableViewOverdue->setEditTriggers(QAbstractItemView::NoEditTriggers);

    ui->tableViewUnpaid->setModel(unpaidModel);
    ui->tableViewUnpaid->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableViewUnpaid->setAlternatingRowColors(true);
    ui->tableViewUnpaid->horizontalHeader()->setStretchLastSection(true);
    ui->tableViewUnpaid->setEditTriggers(QAbstractItemView::NoEditTriggers);

    loadOverdueRentals();
    loadUnpaidPeriods();
}

ExpiryNotificationsForm::~ExpiryNotificationsForm()
{
    delete ui;
}

void ExpiryNotificationsForm::loadOverdueRentals()
{
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("SELECT r.docnumber AS \"Номер\", "
                  "r.docdate AS \"Дата\", "
                  "c.clientname AS \"Клиент\", "
                  "COUNT(rd.terminalid) AS \"Терминалов\", "
                  "(CURRENT_DATE - r.docdate::date) AS \"Дней в аренде\" "
                  "FROM tblrentaldocs r "
                  "JOIN tblclients c ON r.clientid = c.clientid "
                  "JOIN tblrentaldetails rd ON r.rentaldocid = rd.rentaldocid "
                  "WHERE rd.terminalid NOT IN ( "
                  "    SELECT retDet.terminalid FROM tblreturndetails retDet "
                  "    JOIN tblreturndocs ret ON retDet.returndocid = ret.returndocid "
                  "    WHERE ret.clientid = r.clientid "
                  "    AND ret.docdate >= r.docdate "
                  ") "
                  "AND (CURRENT_DATE - r.docdate::date) > 30 "
                  "GROUP BY r.rentaldocid, r.docnumber, r.docdate, c.clientname "
                  "ORDER BY (CURRENT_DATE - r.docdate::date) DESC");

    if (!query.exec()) {
        QMessageBox::critical(this, "Ошибка", query.lastError().text());
        return;
    }

    overdueModel->setQuery(std::move(query));
    ui->tableViewOverdue->resizeColumnsToContents();
}

void ExpiryNotificationsForm::loadUnpaidPeriods()
{
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("SELECT c.clientname AS \"Клиент\", "
                  "r.docnumber AS \"Документ\", "
                  "r.docdate AS \"Дата документа\" "
                  "FROM tblrentaldocs r "
                  "JOIN tblclients c ON r.clientid = c.clientid "
                  "WHERE r.rentaldocid NOT IN ( "
                  "    SELECT pl.rentaldocid FROM tblpayment_rental_links pl "
                  ") "
                  "AND (CURRENT_DATE - r.docdate::date) > 0 "
                  "ORDER BY r.docdate ASC");

    if (!query.exec()) {
        QMessageBox::critical(this, "Ошибка", query.lastError().text());
        return;
    }

    unpaidModel->setQuery(std::move(query));
    ui->tableViewUnpaid->resizeColumnsToContents();
}

void ExpiryNotificationsForm::on_btnRefresh_clicked()
{
    loadOverdueRentals();
    loadUnpaidPeriods();
}

void ExpiryNotificationsForm::on_btnExport_clicked()
{
    QString filePath = ReportExporter::getSaveFilePath(this, "Сохранить уведомления в Excel", "Excel файлы (*.xlsx)");

    if (filePath.isEmpty())
        return;

    ReportExporter::exportModelToExcel(overdueModel, "Просроченные аренды", filePath, this);
}

void ExpiryNotificationsForm::on_btnClose_clicked()
{
    close();
}
