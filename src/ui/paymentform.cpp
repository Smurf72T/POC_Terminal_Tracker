#include "paymentform.h"
#include "ui_paymentform.h"
#include "../database/databasemanager.h"
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QDebug>

PaymentForm::PaymentForm(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PaymentForm)
{
    ui->setupUi(this);
    setWindowTitle("Документ: Отметка оплаты за аренду");
    resize(500, 350);

    // Дата платежа по умолчанию — сегодня
    ui->dateEdit->setDate(QDate::currentDate());

    // Загружаем справочные данные
    loadClients();
    loadMonths();
    loadYears();

    // Сумма по умолчанию 0
    ui->doubleSpinBoxAmount->setValue(0.00);
    ui->doubleSpinBoxAmount->setDecimals(2);
    ui->doubleSpinBoxAmount->setMinimum(0.00);
    ui->doubleSpinBoxAmount->setMaximum(999999.99);
}

PaymentForm::~PaymentForm()
{
    delete ui;
}

void PaymentForm::loadClients()
{
    ui->comboBoxClient->clear();
    ui->comboBoxClient->addItem("-- Выберите клиента --", 0);

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.exec("SELECT clientid, clientname FROM tblclients ORDER BY clientname");

    while (query.next()) {
        ui->comboBoxClient->addItem(query.value(1).toString(), query.value(0).toInt());
    }
}

void PaymentForm::loadMonths()
{
    ui->comboBoxMonth->clear();

    QStringList monthNames = {
        "Январь", "Февраль", "Март", "Апрель", "Май", "Июнь",
        "Июль", "Август", "Сентябрь", "Октябрь", "Ноябрь", "Декабрь"
    };

    for (int i = 0; i < 12; ++i) {
        ui->comboBoxMonth->addItem(monthNames[i], i + 1); // value = 1..12
    }

    // По умолчанию — текущий месяц
    ui->comboBoxMonth->setCurrentIndex(QDate::currentDate().month() - 1);
}

void PaymentForm::loadYears()
{
    int currentYear = QDate::currentDate().year();
    ui->spinBoxYear->setMinimum(currentYear - 2);
    ui->spinBoxYear->setMaximum(currentYear + 2);
    ui->spinBoxYear->setValue(currentYear);
}

bool PaymentForm::checkExistingPayment(int clientId, int month, int year)
{
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("SELECT paymentid, amount FROM tblpayments "
                  "WHERE clientid = :cid AND periodmonth = :month AND periodyear = :year");
    query.bindValue(":cid", clientId);
    query.bindValue(":month", month);
    query.bindValue(":year", year);

    if (query.exec() && query.next()) {
        return true; // Оплата уже существует
    }
    return false;
}

void PaymentForm::on_btnSave_clicked()
{
    int clientId = ui->comboBoxClient->currentData().toInt();
    int month = ui->comboBoxMonth->currentData().toInt();
    int year = ui->spinBoxYear->value();
    double amount = ui->doubleSpinBoxAmount->value();

    // Валидация
    if (clientId == 0) {
        QMessageBox::warning(this, "Внимание", "Выберите клиента!");
        return;
    }
    if (amount <= 0) {
        QMessageBox::warning(this, "Внимание", "Сумма оплаты должна быть больше нуля!");
        return;
    }

    // Проверяем, нет ли уже оплаты за этот период
    if (checkExistingPayment(clientId, month, year)) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "Подтверждение",
            QString("Для этого клиента уже существует оплата за %1 %2 года.\n"
                    "Заменить существующую запись?")
                .arg(ui->comboBoxMonth->currentText(), QString::number(year)),
            QMessageBox::Yes | QMessageBox::No);

        if (reply != QMessageBox::Yes) {
            return;
        }

        // Удаляем старую запись
        QSqlQuery deleteQuery(DatabaseManager::instance().getDatabase());
        deleteQuery.prepare("DELETE FROM tblpayments "
                            "WHERE clientid = :cid AND periodmonth = :month AND periodyear = :year");
        deleteQuery.bindValue(":cid", clientId);
        deleteQuery.bindValue(":month", month);
        deleteQuery.bindValue(":year", year);

        if (!deleteQuery.exec()) {
            QMessageBox::critical(this, "Ошибка БД",
                "Не удалось удалить старую запись: " + deleteQuery.lastError().text());
            return;
        }
    }

    // Вставляем новую запись
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("INSERT INTO tblpayments (clientid, paymentdate, periodmonth, periodyear, amount, comment) "
                  "VALUES (:cid, :date, :month, :year, :amount, :comment)");
    query.bindValue(":cid", clientId);
    query.bindValue(":date", ui->dateEdit->date());
    query.bindValue(":month", month);
    query.bindValue(":year", year);
    query.bindValue(":amount", amount);
    query.bindValue(":comment", ui->textEditComment->toPlainText());

    if (query.exec()) {
        QMessageBox::information(this, "Успех",
            QString("Оплата за %1 %2 года успешно сохранена!")
                .arg(ui->comboBoxMonth->currentText(), QString::number(year)));
        this->close();
    } else {
        QMessageBox::critical(this, "Ошибка БД",
            "Не удалось сохранить оплату: " + query.lastError().text());
    }
}

void PaymentForm::on_btnClose_clicked()
{
    close();
}