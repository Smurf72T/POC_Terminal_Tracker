#include "archivedocumentsform.h"
#include "ui_archivedocumentsform.h"
#include "database/databasemanager.h"
#include "ui/dialogs/receiptform.h"
#include "ui/dialogs/rentalform.h"
#include "ui/dialogs/returnform.h"
#include "ui/dialogs/paymentform.h"
#include "ui/dialogs/statuschangeform.h"
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDate>
#include <QDebug>
#include "utils/reportexporter.h"
#include <QFileDialog>
#include <QTextDocument>

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

    // Загружаем клиентов (если это не Поступление и не Изменение статусов)
    if (m_docType != 1 && m_docType != 5) {
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
    } else if (m_docType == 5) {
        setWindowTitle("Архив: Изменение статусов");
        ui->labelClient->setVisible(false);
        ui->comboBoxClient->setVisible(false);
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
        queryStr = QString("SELECT receiptdocid, "
                           "docnumber AS \"Номер\", "
                           "docdate AS \"Дата\", "
                           "comments AS \"Комментарий\" "
                           "FROM tblreceiptdocs "
                           "WHERE docdate BETWEEN :dateFrom AND :dateTo "
                           "ORDER BY docdate DESC");
    } else if (m_docType == 2) { // Аренда — с возвратом и оплатой
        queryStr = QString(
            "SELECT r.rentaldocid, "
            "r.docnumber AS \"Номер\", "
            "r.docdate AS \"Дата\", "
            "c.clientname AS \"Клиент\", "
            "r.comments AS \"Комментарий\", "
            "COALESCE(ret.returned_cnt, 0)::text || ' из ' || COALESCE(det.total_cnt, 0)::text AS \"Возврат\", "
            "CASE WHEN pay.payment_cnt > 0 THEN 'Оплачено' ELSE 'Не оплачено' END AS \"Оплата\" "
            "FROM tblrentaldocs r "
            "LEFT JOIN tblclients c ON r.clientid = c.clientid "
            "LEFT JOIN (SELECT rentaldocid, COUNT(*) AS total_cnt FROM tblrentaldetails GROUP BY rentaldocid) det ON r.rentaldocid = det.rentaldocid "
            "LEFT JOIN (SELECT rd.rentaldocid, COUNT(DISTINCT rtd.terminalid) AS returned_cnt "
            "          FROM tblreturndetails rtd "
            "          JOIN tblrentaldetails rd ON rtd.terminalid = rd.terminalid "
            "          GROUP BY rd.rentaldocid) ret ON r.rentaldocid = ret.rentaldocid "
            "LEFT JOIN (SELECT rentaldocid, COUNT(*) AS payment_cnt FROM tblpayment_rental_links GROUP BY rentaldocid) pay ON r.rentaldocid = pay.rentaldocid "
            "WHERE r.docdate BETWEEN :dateFrom AND :dateTo "
            "AND (:clientId = 0 OR r.clientid = :clientId) "
            "ORDER BY r.docdate DESC");
    } else if (m_docType == 3) { // Возврат
        queryStr = QString("SELECT r.returndocid, "
                           "r.docnumber AS \"Номер\", "
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
            "SELECT p.paymentid, "
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
    } else if (m_docType == 5) { // Изменение статусов
        queryStr = QString(
            "SELECT sc.statuschangedocid, "
            "sc.docnumber AS \"Номер\", "
            "sc.docdate AS \"Дата\", "
            "CASE sc.actiontype WHEN 'repair' THEN 'В ремонт' WHEN 'repair_return' THEN 'Возврат из ремонта' "
            "WHEN 'writeoff' THEN 'Списан' WHEN 'lost' THEN 'Утерян' ELSE sc.actiontype END AS \"Операция\", "
            "COALESCE(det.cnt, 0)::text || ' терминал(ов)' AS \"Терминалов\", "
            "sc.comment AS \"Комментарий\" "
            "FROM tblstatuschangedocs sc "
            "LEFT JOIN (SELECT statuschangedocid, COUNT(*) AS cnt "
            "          FROM tblstatuschangedetails GROUP BY statuschangedocid) det "
            "ON sc.statuschangedocid = det.statuschangedocid "
            "WHERE sc.docdate BETWEEN :dateFrom AND :dateTo "
            "ORDER BY sc.docdate DESC");
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
    ui->tableView->hideColumn(0);
}

void ArchiveDocumentsForm::on_btnFilter_clicked()
{
    applyFilter();
}

void ArchiveDocumentsForm::on_btnClose_clicked()
{
    close();
}

void ArchiveDocumentsForm::on_tableView_doubleClicked(const QModelIndex &index)
{
    int row = index.row();
    int docId = getDocIdFromRow(row);
    if (docId <= 0) return;

    switch (m_docType) {
        case 1: openReceiptForEdit(docId); break;
        case 2: openRentalForEdit(docId); break;
        case 3: openReturnForEdit(docId); break;
        case 4: openPaymentForEdit(docId); break;
        case 5: openStatusChangeForEdit(docId); break;
    }
}

int ArchiveDocumentsForm::getDocIdFromRow(int row) const
{
    return model->data(model->index(row, 0)).toInt();
}

void ArchiveDocumentsForm::openReceiptForEdit(int docId)
{
    ReceiptForm form(this);
    form.loadForEdit(docId);
    if (form.exec() == QDialog::Accepted) {
        applyFilter();
    }
}

void ArchiveDocumentsForm::openRentalForEdit(int docId)
{
    RentalForm form(this);
    form.loadForEdit(docId);
    if (form.exec() == QDialog::Accepted) {
        applyFilter();
    }
}

void ArchiveDocumentsForm::openReturnForEdit(int docId)
{
    ReturnForm form(this);
    form.loadForEdit(docId);
    if (form.exec() == QDialog::Accepted) {
        applyFilter();
    }
}

void ArchiveDocumentsForm::openPaymentForEdit(int docId)
{
    PaymentForm form(this);
    form.loadForEdit(docId);
    if (form.exec() == QDialog::Accepted) {
        applyFilter();
    }
}

void ArchiveDocumentsForm::openStatusChangeForEdit(int docId)
{
    StatusChangeForm form(this);
    form.loadForEdit(docId);
    if (form.exec() == QDialog::Accepted) {
        applyFilter();
    }
}

void ArchiveDocumentsForm::setupCheckBoxColumn()
{
    // Для архива аренды (docType == 2) колонка "Возврат" (индекс 5)
    if (m_docType == 2) {
        ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    }
}

void ArchiveDocumentsForm::on_btnExportExcel_clicked()
{
    QString filePath = ReportExporter::getSaveFilePath(
        this, "Сохранить отчёт в Excel",
        "Excel файлы (*.xlsx)");

    if (filePath.isEmpty()) return;

    QString title;
    if (m_docType == 1) title = "Архив: Поступление терминалов";
    else if (m_docType == 2) title = "Архив: Передача в аренду";
    else if (m_docType == 3) title = "Архив: Возврат из аренды";
    else if (m_docType == 4) title = "Архив: Оплата";
    else title = "Архив: Изменение статусов";

    ReportExporter::exportModelToExcel(model, title, filePath, this);
}

void ArchiveDocumentsForm::on_btnExportPdf_clicked()
{
    QString filePath = ReportExporter::getSaveFilePath(
        this, "Сохранить отчёт в PDF",
        "PDF файлы (*.pdf)");

    if (filePath.isEmpty()) return;

    // Формируем HTML-таблицу из модели
    QString html = "<html><head><meta charset='utf-8'>"
                   "<style>"
                   "body { font-family: Arial, sans-serif; font-size: 12px; }"
                   "h1 { color: #333; }"
                   "table { border-collapse: collapse; width: 100%; }"
                   "th { background-color: #4472C4; color: white; padding: 8px; border: 1px solid #ddd; }"
                   "td { padding: 6px; border: 1px solid #ddd; }"
                   "tr:nth-child(even) { background-color: #f2f2f2; }"
                   "</style></head><body>";

    if (m_docType == 1) html += "<h1>Архив: Поступление терминалов</h1>";
    else if (m_docType == 2) html += "<h1>Архив: Передача в аренду</h1>";
    else if (m_docType == 3) html += "<h1>Архив: Возврат из аренды</h1>";
    else if (m_docType == 4) html += "<h1>Архив: Оплата</h1>";
    else html += "<h1>Архив: Изменение статусов</h1>";

    html += "<p>Период: с " + ui->dateEditFrom->date().toString("dd.MM.yyyy") +
            " по " + ui->dateEditTo->date().toString("dd.MM.yyyy") + "</p>";
    html += "<p>Дата формирования: " + QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss") + "</p>";

    html += "<table><tr>";
    for (int col = 0; col < model->columnCount(); ++col) {
        html += "<th>" + model->headerData(col, Qt::Horizontal).toString().toHtmlEscaped() + "</th>";
    }
    html += "</tr>";

    for (int row = 0; row < model->rowCount(); ++row) {
        html += "<tr>";
        for (int col = 0; col < model->columnCount(); ++col) {
            QString value = model->data(model->index(row, col)).toString();
            html += "<td>" + value.toHtmlEscaped() + "</td>";
        }
        html += "</tr>";
    }
    html += "</table></body></html>";

    if (ReportExporter::exportHtmlToPdf(html, filePath, this)) {
        QMessageBox::information(this, "Успех", "PDF-отчёт успешно сохранён.");
    } else {
        QMessageBox::critical(this, "Ошибка", "Не удалось сохранить PDF-файл.");
    }
}