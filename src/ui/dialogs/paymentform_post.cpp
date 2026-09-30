#include "paymentform.h"
#include "ui_paymentform.h"
#include "database/databasemanager.h"
#include "services/postactionlogger.h"
#include "ui/base/printservice.h"
#include "ui/base/transactionguard.h"
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDatabase>
#include <QDateTime>
#include <QTime>
#include <QStandardItemModel>
#include <QDebug>
#include "utils/logging.h"

bool PaymentForm::validateBeforePost()
{
    int clientId = ui->comboBoxClient->currentData().toInt();
    int month = ui->comboBoxMonth->currentData().toInt();
    int year = ui->spinBoxYear->value();
    double amount = ui->doubleSpinBoxAmount->value();

    if (clientId == 0) {
        QMessageBox::warning(this, "Внимание", "Выберите клиента!");
        return false;
    }
    if (amount <= 0) {
        QMessageBox::warning(this, "Внимание", "Сумма оплаты должна быть больше нуля!");
        return false;
    }

    // Собираем выбранные документы аренды в член класса: postDetails() берёт
    // их оттуда (раньше здесь объявлялась локальная переменная с тем же именем
    // — из-за затенения связи с документами не сохранялись).
    m_selectedRentalIds.clear();
    QStandardItemModel* listModel = qobject_cast<QStandardItemModel*>(ui->tableViewRentals->model());
    if (listModel) {
        for (int i = 0; i < listModel->rowCount(); ++i) {
            QStandardItem* item = listModel->item(i, ColRental);
            if (item && item->checkState() == Qt::Checked) {
                m_selectedRentalIds.append(item->data(kRentalIdRole).toInt());
            }
        }
    }
    return true;
}

int PaymentForm::postHeader(QSqlDatabase& db)
{
    int clientId = ui->comboBoxClient->currentData().toInt();
    int month = ui->comboBoxMonth->currentData().toInt();
    int year = ui->spinBoxYear->value();
    double amount = ui->doubleSpinBoxAmount->value();
    int paymentId = m_editDocId;

    if (m_editMode) {
        // Режим редактирования — UPDATE существующего платежа
        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE tblpayments SET clientid = :cid, paymentdate = :date, "
                            "periodmonth = :month, periodyear = :year, amount = :amount, comment = :comment "
                            "WHERE paymentid = :id");
        updateQuery.bindValue(":cid", clientId);
        updateQuery.bindValue(":date", QDateTime(ui->dateEdit->date(), QTime::currentTime()));
        updateQuery.bindValue(":month", month);
        updateQuery.bindValue(":year", year);
        updateQuery.bindValue(":amount", amount);
        updateQuery.bindValue(":comment", ui->textEditComment->toPlainText());
        updateQuery.bindValue(":id", paymentId);

        if (!updateQuery.exec()) {
            QMessageBox::critical(this, "Ошибка БД", "Не удалось обновить платёж: " + updateQuery.lastError().text());
            return -1;
        }

        // Удаляем старые связи
        QSqlQuery deleteLinks(db);
        deleteLinks.prepare("DELETE FROM tblpayment_rental_links WHERE paymentid = :id");
        deleteLinks.bindValue(":id", paymentId);
        if (!deleteLinks.exec()) {
            QMessageBox::critical(this, "Ошибка БД", "Не удалось обновить связи: " + deleteLinks.lastError().text());
            return -1;
        }
    } else {
        // Режим создания. Оплат за один период может быть несколько (клиент
        // платит дважды в месяце) — новая оплата просто добавляется к
        // существующим, ничего не заменяя. Ограничение UNIQUE
        // (clientid, periodmonth, periodyear) снято миграцией 016.
        QSqlQuery query(db);
        query.prepare("INSERT INTO tblpayments (clientid, paymentdate, periodmonth, periodyear, amount, comment) "
                      "VALUES (:cid, :date, :month, :year, :amount, :comment) RETURNING paymentid");
        query.bindValue(":cid", clientId);
        query.bindValue(":date", QDateTime(ui->dateEdit->date(), QTime::currentTime()));
        query.bindValue(":month", month);
        query.bindValue(":year", year);
        query.bindValue(":amount", amount);
        query.bindValue(":comment", ui->textEditComment->toPlainText());

        if (!query.exec() || !query.next()) {
            QMessageBox::critical(this, "Ошибка БД", "Не удалось сохранить оплату: " + query.lastError().text());
            return -1;
        }
        paymentId = query.value(0).toInt();
    }
    return paymentId;
}

bool PaymentForm::postDetails(QSqlDatabase& db, int docId)
{
    for (int rentalId : m_selectedRentalIds) {
        QSqlQuery linkQuery(db);
        linkQuery.prepare("INSERT INTO tblpayment_rental_links (paymentid, rentaldocid) VALUES (:pid, :rid)");
        linkQuery.bindValue(":pid", docId);
        linkQuery.bindValue(":rid", rentalId);

        if (!linkQuery.exec()) {
            QMessageBox::critical(this, "Ошибка БД", "Не удалось создать связь: " + linkQuery.lastError().text());
            return false;
        }
    }
    return true;
}
