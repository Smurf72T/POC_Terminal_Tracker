#include "rentalform.h"
#include "ui_rentalform.h"
#include "comboboxdelegate.h"
#include "comboboxmodel.h"
#include "../database/databasemanager.h"
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QDebug>
#include <QSqlRecord>

RentalForm::RentalForm(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::RentalForm)
{
    ui->setupUi(this);
    setWindowTitle("Документ: Передача в аренду");
    resize(900, 600);

    // Настройка даты (сегодня)
    ui->dateEdit->setDate(QDate::currentDate());

    // Генерация номера
    generateDocNumber();

    // Настройка модели для табличной части
    rowsModel = new QStandardItemModel(0, 3, this); // 3 колонки
    rowsModel->setHorizontalHeaderLabels({"Терминал", "SIM-карта", "Примечание"});
    ui->tableView->setModel(rowsModel);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);

    // Загружаем данные для выпадающих списков
    loadClientsToDelegate();
    loadFreeTerminalsToDelegate();
    loadFreeSIMsToDelegate();

    // Подключаем сигнал изменения данных
    connect(rowsModel, &QStandardItemModel::dataChanged,
            this, &RentalForm::onTableViewDataChanged);
}

RentalForm::~RentalForm()
{
    delete ui;
}

void RentalForm::loadClientsToDelegate()
{
    QList<QPair<int, QString>> clients;
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.exec("SELECT clientid, clientname FROM tblclients ORDER BY clientname");

    while (query.next()) {
        clients.append(qMakePair(query.value(0).toInt(), query.value(1).toString()));
    }

    // Устанавливаем делегат для колонки клиента
    ui->comboBoxClient->setItemDelegate(new ComboBoxDelegate(clients, this));
    ui->comboBoxClient->setModel(new ComboBoxModel(clients, this));
}

void RentalForm::loadFreeTerminalsToDelegate()
{
    // Загрузим только свободные терминалы
    QList<QPair<int, QString>> terminals;
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.exec("SELECT terminalid, serialnumber FROM tblterminals WHERE status = 0 ORDER BY serialnumber");

    while (query.next()) {
        terminals.append(qMakePair(query.value(0).toInt(), query.value(1).toString()));
    }

    // Устанавливаем делегат на колонку терминала
    ui->tableView->setItemDelegateForColumn(0, new ComboBoxDelegate(terminals, this));
}

void RentalForm::loadFreeSIMsToDelegate()
{
    QList<QPair<int, QString>> sims;
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.exec("SELECT simcardid, simnumber FROM tblsimcards WHERE status = 0 ORDER BY simnumber");

    while (query.next()) {
        sims.append(qMakePair(query.value(0).toInt(), query.value(1).toString()));
    }

    // Устанавливаем делегат на колонку SIM
    ui->tableView->setItemDelegateForColumn(1, new ComboBoxDelegate(sims, this));
}

void RentalForm::generateDocNumber()
{
    ui->lineEditNumber->setText("АР-" + QString::number(QDateTime::currentMSecsSinceEpoch() % 100000));
}

void RentalForm::on_btnAddRow_clicked()
{
    int row = rowsModel->rowCount();
    rowsModel->insertRow(row);

    // Создаем элементы с пустым текстом и ID = 0
    QStandardItem *terminalItem = new QStandardItem();
    terminalItem->setData(0, Qt::UserRole); // ID терминала
    terminalItem->setData("", Qt::DisplayRole); // Текст для отображения

    QStandardItem *simItem = new QStandardItem();
    simItem->setData(0, Qt::UserRole); // ID SIM
    simItem->setData("", Qt::DisplayRole); // Текст для отображения

    QStandardItem *commentItem = new QStandardItem("");

    rowsModel->setItem(row, 0, terminalItem);
    rowsModel->setItem(row, 1, simItem);
    rowsModel->setItem(row, 2, commentItem);
}

void RentalForm::on_btnDeleteRow_clicked()
{
    int row = ui->tableView->currentIndex().row();
    if (row >= 0) {
        rowsModel->removeRow(row);
    }
}

void RentalForm::on_btnPost_clicked()
{
    if (rowsModel->rowCount() == 0) {
        QMessageBox::warning(this, "Внимание", "Добавьте хотя бы одну строку!");
        return;
    }

    int clientId = ui->comboBoxClient->currentData().toInt();
    if (clientId == 0) {
        QMessageBox::warning(this, "Внимание", "Выберите клиента!");
        return;
    }

    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    if (!db.transaction()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось начать транзакцию");
        return;
    }

    QSqlQuery query(db);

    // 1. Создаем шапку документа
    query.prepare("INSERT INTO tblrentaldocs (docnumber, docdate, clientid, comments) "
                  "VALUES (:num, :date, :client, :comm) RETURNING rentaldocid");
    query.bindValue(":num", ui->lineEditNumber->text());
    query.bindValue(":date", ui->dateEdit->date());
    query.bindValue(":client", clientId);
    query.bindValue(":comm", ui->textEditComment->toPlainText());

    if (!query.exec() || !query.next()) {
        db.rollback();
        QMessageBox::critical(this, "Ошибка БД", "Не удалось создать шапку: " + query.lastError().text());
        return;
    }
    int docId = query.value(0).toInt();

    // 2. Обрабатываем строки (главная часть с защитой от гонки)
    bool error = false;
    for (int i = 0; i < rowsModel->rowCount(); ++i) {
        int terminalId = rowsModel->data(rowsModel->index(i, 0), Qt::UserRole).toInt();
        int simId = rowsModel->data(rowsModel->index(i, 1), Qt::UserRole).toInt();
        QString comment = rowsModel->data(rowsModel->index(i, 2), Qt::DisplayRole).toString();

        // Проверяем, что терминал всё ещё свободен
        QSqlQuery checkQuery(db);
        checkQuery.prepare("SELECT status FROM tblterminals WHERE terminalid = :id FOR UPDATE NOWAIT");
        checkQuery.bindValue(":id", terminalId);

        if (!checkQuery.exec() || !checkQuery.next()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка",
                QString("Не удалось заблокировать терминал %1. Возможно, он уже сдан в аренду.").arg(terminalId));
            return;
        }

        int status = checkQuery.value(0).toInt();
        if (status != 0) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка",
                QString("Терминал %1 больше не свободен!").arg(terminalId));
            return;
        }

        // Обновляем статус терминала и устанавливаем SIM
        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE tblterminals SET status = 1, currentsimcardid = :simid WHERE terminalid = :id");
        updateQuery.bindValue(":id", terminalId);
        updateQuery.bindValue(":simid", simId);

        if (!updateQuery.exec()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД",
                QString("Не удалось обновить терминал %1: %2").arg(terminalId).arg(updateQuery.lastError().text()));
            return;
        }

        // Обновляем статус SIM-карты
        QSqlQuery simQuery(db);
        simQuery.prepare("UPDATE tblsimcards SET status = 1 WHERE simcardid = :id");
        simQuery.bindValue(":id", simId);

        if (!simQuery.exec()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД",
                QString("Не удалось обновить SIM-карту %1: %2").arg(simId).arg(simQuery.lastError().text()));
            return;
        }

        // Создаем запись в детали документа
        QSqlQuery detailQuery(db);
        detailQuery.prepare("INSERT INTO tblrentaldetails (rentaldocid, terminalid, simcardid) "
                            "VALUES (:did, :tid, :sid)");
        detailQuery.bindValue(":did", docId);
        detailQuery.bindValue(":tid", terminalId);
        detailQuery.bindValue(":sid", simId);

        if (!detailQuery.exec()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД", "Ошибка связи: " + detailQuery.lastError().text());
            return;
        }
    }

    // 3. Фиксируем транзакцию
    if (!db.commit()) {
        db.rollback();
        QMessageBox::critical(this, "Ошибка", "Не удалось зафиксировать транзакцию");
    } else {
        QMessageBox::information(this, "Успех", "Документ успешно проведен!");
        DatabaseManager::instance().notifyDataChanged();
        this->close();
    }
}

void RentalForm::onTableViewDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    // Когда данные изменились, обновляем отображение
    Q_UNUSED(bottomRight);

    int row = topLeft.row();
    int column = topLeft.column();

    // Если изменилась колонка терминала или SIM
    if (column == 0 || column == 1) {
        // Принудительно обновляем отображение
        QModelIndex index = rowsModel->index(row, column);
        Q_UNUSED(index);
    }
}

void RentalForm::on_btnClose_clicked()
{
    close();
}