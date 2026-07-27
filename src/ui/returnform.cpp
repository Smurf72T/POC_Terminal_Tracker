#include "returnform.h"
#include "ui_returnform.h"
#include "../database/databasemanager.h"
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QDebug>

ReturnForm::ReturnForm(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ReturnForm)
{
    ui->setupUi(this);
    setWindowTitle("Документ: Возврат из аренды");
    resize(900, 600);

    ui->dateEdit->setDate(QDate::currentDate());
    generateDocNumber();

    // Настройка модели таблицы
    rowsModel = new QStandardItemModel(0, 3, this);
    // Колонки: 0 - Возврат (чекбокс), 1 - Терминал, 2 - SIM-карта
    rowsModel->setHorizontalHeaderLabels({"Возврат", "Терминал", "SIM-карта"});
    ui->tableView->setModel(rowsModel);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);

    // Загружаем клиентов
    loadClientsToComboBox();
}

ReturnForm::~ReturnForm()
{
    delete ui;
}

void ReturnForm::loadClientsToComboBox()
{
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.exec("SELECT clientid, clientname FROM tblclients ORDER BY clientname");

    while (query.next()) {
        ui->comboBoxClient->addItem(query.value(1).toString(), query.value(0).toInt());
    }
}

void ReturnForm::loadRentalDocs(int clientId)
{
    // Очищаем ComboBox документов
    ui->comboBoxRentalDoc->clear();
    rowsModel->removeRows(0, rowsModel->rowCount());

    if (clientId == 0) return;

    // Загружаем документы аренды для этого клиента
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("SELECT rentaldocid, docnumber, docdate FROM tblrentaldocs "
                  "WHERE clientid = :cid ORDER BY docdate DESC");
    query.bindValue(":cid", clientId);

    if (!query.exec()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось загрузить документы: " + query.lastError().text());
        return;
    }

    while (query.next()) {
        int docId = query.value(0).toInt();
        QString docNumber = query.value(1).toString();
        QString docDate = query.value(2).toDateTime().toString("dd.MM.yyyy");

        QString displayText = QString("%1 от %2").arg(docNumber, docDate);
        ui->comboBoxRentalDoc->addItem(displayText, docId);
    }
}

void ReturnForm::loadRentalDetails(int rentalDocId)
{
    rowsModel->removeRows(0, rowsModel->rowCount());
    if (rentalDocId == 0) return;

    // Загружаем строки из документа аренды
    // Показываем все терминалы из документа, даже если они уже возвращены (для истории)
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare(
        "SELECT rd.terminalid, t.serialnumber, "
        "COALESCE(s.simnumber, 'Нет SIM') AS simnumber, "
        "t.status AS terminal_status, "
        "s.status AS sim_status "
        "FROM tblrentaldetails rd "
        "JOIN tblterminals t ON rd.terminalid = t.terminalid "
        "LEFT JOIN tblsimcards s ON rd.simcardid = s.simcardid "
        "WHERE rd.rentaldocid = :docid "
        "ORDER BY t.serialnumber"
    );
    query.bindValue(":docid", rentalDocId);

    if (!query.exec()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось загрузить детали: " + query.lastError().text());
        return;
    }

    while (query.next()) {
        int row = rowsModel->rowCount();
        rowsModel->insertRow(row);

        // Колонка 0: Чекбокс "Возврат"
        QStandardItem *checkItem = new QStandardItem();
        checkItem->setCheckable(true);
        checkItem->setCheckState(Qt::Unchecked);
        checkItem->setData(query.value(0).toInt(), Qt::UserRole); // Храним ID терминала
        rowsModel->setItem(row, 0, checkItem);

        // Колонка 1: Терминал (только для чтения)
        QStandardItem *termItem = new QStandardItem(query.value(1).toString());
        termItem->setEditable(false);
        termItem->setData(query.value(0).toInt(), Qt::UserRole);
        rowsModel->setItem(row, 1, termItem);

        // Колонка 2: SIM (только для чтения)
        QStandardItem *simItem = new QStandardItem(query.value(2).toString());
        simItem->setEditable(false);
        rowsModel->setItem(row, 2, simItem);
    }
}

void ReturnForm::on_comboBoxClient_currentIndexChanged(int index)
{
    int clientId = ui->comboBoxClient->itemData(index).toInt();
    loadRentalDocs(clientId);
}

void ReturnForm::on_comboBoxRentalDoc_currentIndexChanged(int index)
{
    int rentalDocId = ui->comboBoxRentalDoc->itemData(index).toInt();
    loadRentalDetails(rentalDocId);
}

void ReturnForm::generateDocNumber()
{
    ui->lineEditNumber->setText("ВР-" + QString::number(QDateTime::currentMSecsSinceEpoch() % 100000));
}

void ReturnForm::on_btnPost_clicked()
{
    int clientId = ui->comboBoxClient->currentData().toInt();
    int rentalDocId = ui->comboBoxRentalDoc->currentData().toInt();

    if (clientId == 0) {
        QMessageBox::warning(this, "Внимание", "Выберите клиента!");
        return;
    }
    if (rentalDocId == 0) {
        QMessageBox::warning(this, "Внимание", "Выберите документ аренды!");
        return;
    }

    // Собираем ID терминалов, которые нужно вернуть
    QList<int> terminalsToReturn;
    for (int i = 0; i < rowsModel->rowCount(); ++i) {
        QStandardItem *checkItem = rowsModel->item(i, 0);
        if (checkItem && checkItem->checkState() == Qt::Checked) {
            terminalsToReturn.append(checkItem->data(Qt::UserRole).toInt());
        }
    }

    if (terminalsToReturn.isEmpty()) {
        QMessageBox::warning(this, "Внимание", "Отметьте хотя бы один терминал для возврата!");
        return;
    }

    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    if (!db.transaction()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось начать транзакцию");
        return;
    }

    QSqlQuery query(db);

    // 1. Создаем шапку
    query.prepare("INSERT INTO tblreturndocs (docnumber, docdate, clientid, comments) "
                  "VALUES (:num, :date, :client, :comm) RETURNING returndocid");
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

    // 2. Обрабатываем выбранные терминалы
    for (int termId : terminalsToReturn) {
        // Проверяем, что терминал всё ещё в аренде (защита от гонки)
        QSqlQuery checkQuery(db);
        checkQuery.prepare("SELECT status FROM tblterminals WHERE terminalid = :id FOR UPDATE NOWAIT");
        checkQuery.bindValue(":id", termId);

        if (!checkQuery.exec() || !checkQuery.next()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка",
                QString("Не удалось заблокировать терминал %1").arg(termId));
            return;
        }

        int status = checkQuery.value(0).toInt();
        if (status != 1) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка",
                QString("Терминал %1 уже не находится в аренде!").arg(termId));
            return;
        }

        // Получаем simcardid из деталей аренды для этого терминала
        QSqlQuery simQuery(db);
        simQuery.prepare("SELECT simcardid FROM tblrentaldetails WHERE rentaldocid = :docid AND terminalid = :tid");
        simQuery.bindValue(":docid", rentalDocId);
        simQuery.bindValue(":tid", termId);

        int simcardId = -1;
        if (simQuery.exec() && simQuery.next()) {
            simcardId = simQuery.value(0).toInt();
        }

        // Меняем статус терминала на "Свободен" (0)
        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE tblterminals SET status = 0, currentsimcardid = NULL WHERE terminalid = :id");
        updateQuery.bindValue(":id", termId);

        if (!updateQuery.exec()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД",
                QString("Не удалось обновить статус терминала %1: %2").arg(QString::number(termId)).arg(updateQuery.lastError().text()));
            return;
        }

        // ВАЖНО: Сбрасываем статус сим-карты на "Свободна" (0) ГАРАНТИРОВАННО:
        // Получаем simcardid не из деталей аренды (может быть устаревшим),
        // а из tblterminals.currentsimcardid (там актуальное значение)
        QSqlQuery currentSimQuery(db);
        currentSimQuery.prepare("SELECT currentsimcardid FROM tblterminals WHERE terminalid = :tid");
        currentSimQuery.bindValue(":tid", termId);

        int actualSimId = -1;
        if (currentSimQuery.exec() && currentSimQuery.next()) {
            actualSimId = currentSimQuery.value(0).toInt();
        }

        // Если есть привязанная SIM-карта, сбрасываем её статус
        if (actualSimId > 0) {
            QSqlQuery simUpdateQuery(db);
            simUpdateQuery.prepare("UPDATE tblsimcards SET status = 0 WHERE simcardid = :id");
            simUpdateQuery.bindValue(":id", actualSimId);

            if (!simUpdateQuery.exec()) {
                db.rollback();
                QMessageBox::critical(this, "Ошибка БД",
                    QString("Не удалось обновить статус SIM-карты %1: %2").arg(QString::number(actualSimId)).arg(simUpdateQuery.lastError().text()));
                return;
            }
        }

        // Записываем в детали возврата
        QSqlQuery detailQuery(db);
        detailQuery.prepare("INSERT INTO tblreturndetails (returndocid, terminalid) VALUES (:did, :tid)");
        detailQuery.bindValue(":did", docId);
        detailQuery.bindValue(":tid", termId);

        if (!detailQuery.exec()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД", "Ошибка связи: " + detailQuery.lastError().text());
            return;
        }
    }

    // 3. Фиксируем
    if (!db.commit()) {
        db.rollback();
        QMessageBox::critical(this, "Ошибка", "Не удалось зафиксировать транзакцию");
    } else {
        QMessageBox::information(this, "Успех", "Возврат успешно проведен!");
        DatabaseManager::instance().notifyDataChanged();
        this->close();
    }
}

void ReturnForm::on_btnClose_clicked()
{
    close();
}