#include "archivedocumentsform.h"
#include "ui_archivedocumentsform.h"
#include "../database/databasemanager.h"
#include "CheckBoxDelegate.h"
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDate>
#include <QDebug>

ArchiveDocumentsForm::ArchiveDocumentsForm(int docType, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ArchiveDocumentsForm),
    model(new QSqlQueryModel(this)),
    m_docType(docType)
{
    ui->setupUi(this);
    setupUI();

    // Устанавливаем даты по умолчанию (текущий месяц)
    QDate today = QDate::currentDate();
    ui->dateEditFrom->setDate(QDate(today.year(), today.month(), 1));
    ui->dateEditTo->setDate(today);

    // Загружаем клиентов (если это не Поступление)
    if (m_docType != 1) {
        loadClients();
    }

    // Сразу применяем фильтр
    applyFilter();
}

ArchiveDocumentsForm::~ArchiveDocumentsForm()
{
    delete ui;
}

void ArchiveDocumentsForm::setupUI()
{
    if (m_docType == 1) {
        setWindowTitle("Архив: Поступление терминалов");
        ui->labelClient->setVisible(false);
        ui->comboBoxClient->setVisible(false);
    } else if (m_docType == 2) {
        setWindowTitle("Архив: Передача в аренду");
    } else if (m_docType == 3) {
        setWindowTitle("Архив: Возврат из аренды");
    } else if (m_docType == 4) {
        setWindowTitle("Архив: Оплата");
    }

    ui->tableView->setModel(model);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setAlternatingRowColors(true);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);

    setupCheckBoxColumn();
}

void ArchiveDocumentsForm::loadClients()
{
    ui->comboBoxClient->clear();
    ui->comboBoxClient->addItem("Все клиенты", 0); // Опция "Все"

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.exec("SELECT clientid, clientname FROM tblclients ORDER BY clientname");

    while (query.next()) {
        ui->comboBoxClient->addItem(query.value(1).toString(), query.value(0).toInt());
    }
}

void ArchiveDocumentsForm::applyFilter()
{
    QString queryStr;
    QString dateFrom = ui->dateEditFrom->date().toString("yyyy-MM-dd");
    QString dateTo = ui->dateEditTo->date().toString("yyyy-MM-dd");
    int clientId = ui->comboBoxClient->currentData().toInt();

    if (m_docType == 1) { // Поступление
        queryStr = QString("SELECT docnumber AS \"Номер\", "
                           "docdate AS \"Дата\", "
                           "comments AS \"Комментарий\" "
                           "FROM tblreceiptdocs "
                           "WHERE docdate BETWEEN :dateFrom AND :dateTo "
                           "ORDER BY docdate DESC");
    } else if (m_docType == 2) { // Аренда — с возвратом и оплатой
        queryStr = QString(
            "SELECT r.docnumber AS \"Номер\", "
            "r.docdate AS \"Дата\", "
            "c.clientname AS \"Клиент\", "
            "r.comments AS \"Комментарий\", "
            "COUNT(DISTINCT rd_returned.terminalid)::text || ' из ' || COUNT(DISTINCT rd_rental.terminalid)::text AS \"Возврат\", "
            "CASE WHEN COUNT(pl.linkid) > 0 THEN 'Оплачено' ELSE 'Не оплачено' END AS \"Оплата\" "
            "FROM tblrentaldocs r "
            "LEFT JOIN tblclients c ON r.clientid = c.clientid "
            "LEFT JOIN tblrentaldetails rd_rental ON r.rentaldocid = rd_rental.rentaldocid "
            "LEFT JOIN tblreturndetails rd_returned ON rd_rental.terminalid = rd_returned.terminalid "
            "LEFT JOIN tblpayment_rental_links pl ON r.rentaldocid = pl.rentaldocid "
            "WHERE r.docdate BETWEEN :dateFrom AND :dateTo "
            "AND (:clientId = 0 OR r.clientid = :clientId) "
            "GROUP BY r.rentaldocid, r.docnumber, r.docdate, c.clientname, r.comments "
            "ORDER BY r.docdate DESC");
    } else if (m_docType == 3) { // Возврат
        queryStr = QString("SELECT r.docnumber AS \"Номер\", "
                           "r.docdate AS \"Дата\", "
                           "c.clientname AS \"Клиент\", "
                           "r.comments AS \"Комментарий\" "
                           "FROM tblreturndocs r "
                           "LEFT JOIN tblclients c ON r.clientid = c.clientid "
                           "WHERE r.docdate BETWEEN :dateFrom AND :dateTo "
                           "AND (:clientId = 0 OR r.clientid = :clientId) "
                           "ORDER BY r.docdate DESC");
    } else if (m_docType == 4) { // Оплата
        queryStr = QString(
            "SELECT p.paymentid AS \"ID\", "
            "p.paymentdate AS \"Дата\", "
            "c.clientname AS \"Клиент\", "
            "pm.monthname AS \"Месяц\", "
            "p.periodyear AS \"Год\", "
            "p.amount AS \"Сумма\", "
            "p.comment AS \"Комментарий\" "
            "FROM tblpayments p "
            "LEFT JOIN tblclients c ON p.clientid = c.clientid "
            "LEFT JOIN (VALUES "
            "(1, 'Январь'), (2, 'Февраль'), (3, 'Март'), (4, 'Апрель'), "
            "(5, 'Май'), (6, 'Июнь'), (7, 'Июль'), (8, 'Август'), "
            "(9, 'Сентябрь'), (10, 'Октябрь'), (11, 'Ноябрь'), (12, 'Декабрь') "
            ") AS pm(monthnum, monthname) ON p.periodmonth = pm.monthnum "
            "WHERE p.paymentdate BETWEEN :dateFrom AND :dateTo "
            "AND (:clientId = 0 OR p.clientid = :clientId) "
            "ORDER BY p.paymentdate DESC");
    }

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare(queryStr);
    query.bindValue(":dateFrom", dateFrom);
    query.bindValue(":dateTo", dateTo);
    query.bindValue(":clientId", clientId);

    if (!query.exec()) {
        QMessageBox::critical(this, "Ошибка БД", query.lastError().text());
        return;
    }

    model->setQuery(std::move(query));
}

void ArchiveDocumentsForm::on_btnFilter_clicked()
{
    applyFilter();
}

void ArchiveDocumentsForm::on_btnClose_clicked()
{
    close();
}

void ArchiveDocumentsForm::setupCheckBoxColumn()
{
    // Для архива аренды (docType == 2) настраиваем отображение чекбокса
    if (m_docType == 2) {
        // Колонка "Возврат" — теперь индекс 4 (Номер, Дата, Клиент, Комментарий, Возврат, Оплата)
        int returnColumn = 4;

        // Устанавливаем делегат для отображения чекбокса
        ui->tableView->setItemDelegateForColumn(returnColumn, new CheckBoxDelegate(this));

        // Делаем колонку только для чтения
        ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    }
}