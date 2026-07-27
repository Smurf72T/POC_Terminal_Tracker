#include "archivedocumentsform.h"
#include "ui_archivedocumentsform.h"
#include "../database/databasemanager.h"
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDate>
#include <QDebug>

ArchiveDocumentsForm::ArchiveDocumentsForm(int docType, QWidget *parent) :
    QWidget(parent),
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
    }

    ui->tableView->setModel(model);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setAlternatingRowColors(true);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
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

    // Формируем запрос в зависимости от типа документа
    if (m_docType == 1) { // Поступление
        queryStr = QString("SELECT docnumber AS \"Номер\", "
                           "docdate AS \"Дата\", "
                           "comments AS \"Комментарий\" "
                           "FROM tblreceiptdocs "
                           "WHERE docdate BETWEEN '%1' AND '%2' "
                           "ORDER BY docdate DESC")
                       .arg(dateFrom, dateTo);
    } else if (m_docType == 2) { // Аренда
        queryStr = QString("SELECT r.docnumber AS \"Номер\", "
                           "r.docdate AS \"Дата\", "
                           "c.clientname AS \"Клиент\", "
                           "r.comments AS \"Комментарий\" "
                           "FROM tblrentaldocs r "
                           "LEFT JOIN tblclients c ON r.clientid = c.clientid "
                           "WHERE r.docdate BETWEEN '%1' AND '%2' "
                           "AND (%3 = 0 OR r.clientid = %3) "
                           "ORDER BY r.docdate DESC")
                       .arg(dateFrom, dateTo).arg(clientId);
    } else if (m_docType == 3) { // Возврат
        queryStr = QString("SELECT r.docnumber AS \"Номер\", "
                           "r.docdate AS \"Дата\", "
                           "c.clientname AS \"Клиент\", "
                           "r.comments AS \"Комментарий\" "
                           "FROM tblreturndocs r "
                           "LEFT JOIN tblclients c ON r.clientid = c.clientid "
                           "WHERE r.docdate BETWEEN '%1' AND '%2' "
                           "AND (%3 = 0 OR r.clientid = %3) "
                           "ORDER BY r.docdate DESC")
                       .arg(dateFrom, dateTo).arg(clientId);
    }

    model->setQuery(queryStr, DatabaseManager::instance().getDatabase());

    if (model->lastError().isValid()) {
        QMessageBox::critical(this, "Ошибка БД", model->lastError().text());
    }
}

void ArchiveDocumentsForm::on_btnFilter_clicked()
{
    applyFilter();
}

void ArchiveDocumentsForm::on_btnClose_clicked()
{
    close();
}