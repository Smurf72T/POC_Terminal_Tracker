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

    // Собираем выбранные документы аренды
    QList<int> m_selectedRentalIds;
    QStandardItemModel* listModel = qobject_cast<QStandardItemModel*>(ui->listViewRentals->model());
    if (listModel) {
        for (int i = 0; i < listModel->rowCount(); ++i) {
            QStandardItem* item = listModel->item(i);
            if (item && item->checkState() == Qt::Checked) {
                m_selectedRentalIds.append(item->data(Qt::UserRole).toInt());
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
        // Режим создания — проверка дубликата
        if (checkExistingPayment(clientId, month, year)) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "Подтверждение",
                QString("Оплата за %1 %2 года уже существует. Заменить её (включая привязанные документы)?")
                    .arg(ui->comboBoxMonth->currentText(), QString::number(year)),
                QMessageBox::Yes | QMessageBox::No);

            if (reply != QMessageBox::Yes) {
                return -1;
            }

            QSqlQuery deleteQuery(db);
            deleteQuery.prepare("DELETE FROM tblpayments "
                                "WHERE clientid = :cid AND periodmonth = :month AND periodyear = :year");
            deleteQuery.bindValue(":cid", clientId);
            deleteQuery.bindValue(":month", month);
            deleteQuery.bindValue(":year", year);

            if (!deleteQuery.exec()) {
                QMessageBox::critical(this, "Ошибка БД",
                                      "Не удалось удалить старую запись: " + deleteQuery.lastError().text());
                return -1;
            }
        }

        // Вставляем новую оплату
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
