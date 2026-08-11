#include "returnform.h"
#include "ui_returnform.h"
#include "database/databasemanager.h"
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QTime>
#include <QDebug>
#include <QPrinter>
#include "utils/logging.h"
#include <QPrintDialog>
#include <QTextDocument>

ReturnForm::ReturnForm(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ReturnForm)
{
    ui->setupUi(this);
    setWindowTitle("Документ: Возврат из аренды");
    resize(900, 600);

    ui->dateEdit->setDate(QDate::currentDate());

    // Номер документа генерируется при проведении (не здесь), чтобы не
    // сжигать значения последовательности для отменённых форм.

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
    if (!query.exec("SELECT clientid, clientname FROM tblclients ORDER BY clientname")) {
        qCWarning(logSQL) << "Failed to load clients:" << query.lastError().text();
        return;
    }

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

void ReturnForm::loadForEdit(int docId)
{
    m_editMode = true;
    m_editDocId = docId;
    m_originalReturned.clear();
    m_editRentalDocId = 0;

    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    QSqlQuery query(db);

    query.prepare("SELECT docnumber, docdate, clientid, comments FROM tblreturndocs WHERE returndocid = :id");
    query.bindValue(":id", docId);

    if (!query.exec() || !query.next()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось загрузить документ возврата");
        return;
    }

    QString docNumber = query.value(0).toString();
    QDateTime docDate = query.value(1).toDateTime();
    int clientId = query.value(2).toInt();
    QString comments = query.value(3).toString();

    ui->lineEditNumber->setText(docNumber);
    ui->lineEditNumber->setReadOnly(true);
    ui->dateEdit->setDate(docDate.date());
    ui->textEditComment->setText(comments);
    setWindowTitle(QString("Редактирование возврата ID %1").arg(docId));

    int clientIndex = ui->comboBoxClient->findData(clientId);
    if (clientIndex >= 0) {
        ui->comboBoxClient->setCurrentIndex(clientIndex);
    }

    QSqlQuery rentalQuery(db);
    rentalQuery.prepare(
        "SELECT DISTINCT rd.rentaldocid "
        "FROM tblrentaldetails rd "
        "JOIN tblreturndetails rtd ON rd.terminalid = rtd.terminalid "
        "WHERE rtd.returndocid = :id "
        "LIMIT 1");
    rentalQuery.bindValue(":id", docId);

    if (rentalQuery.exec() && rentalQuery.next()) {
        int rentalDocId = rentalQuery.value(0).toInt();
        m_editRentalDocId = rentalDocId;
        int rentalIndex = ui->comboBoxRentalDoc->findData(rentalDocId);
        if (rentalIndex >= 0) {
            ui->comboBoxRentalDoc->setCurrentIndex(rentalIndex);
        }
    }

    QSqlQuery termQuery(db);
    termQuery.prepare("SELECT terminalid FROM tblreturndetails WHERE returndocid = :id");
    termQuery.bindValue(":id", docId);

    if (termQuery.exec()) {
        QList<int> returnedTerminals;
        while (termQuery.next()) {
            returnedTerminals.append(termQuery.value(0).toInt());
        }
        for (int tid : returnedTerminals)
            m_originalReturned.insert(tid);

        for (int i = 0; i < rowsModel->rowCount(); ++i) {
            QStandardItem *checkItem = rowsModel->item(i, 0);
            if (checkItem) {
                int termId = checkItem->data(Qt::UserRole).toInt();
                if (returnedTerminals.contains(termId)) {
                    checkItem->setCheckState(Qt::Checked);
                }
            }
        }
    }
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

    if (m_editMode) {
        // Режим редактирования: обновляем шапку и детали
        query.prepare("UPDATE tblreturndocs SET docdate = :date, clientid = :client, comments = :comm WHERE returndocid = :id");
        query.bindValue(":date", QDateTime(ui->dateEdit->date(), QTime::currentTime()));
        query.bindValue(":client", clientId);
        query.bindValue(":comm", ui->textEditComment->toPlainText());
        query.bindValue(":id", m_editDocId);

        if (!query.exec()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД", "Не удалось обновить шапку: " + query.lastError().text());
            return;
        }

        query.prepare("DELETE FROM tblreturndetails WHERE returndocid = :id");
        query.bindValue(":id", m_editDocId);
        if (!query.exec()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД", "Не удалось удалить детали: " + query.lastError().text());
            return;
        }

        for (int termId : terminalsToReturn) {
            query.prepare("INSERT INTO tblreturndetails (returndocid, terminalid) VALUES (:did, :tid)");
            query.bindValue(":did", m_editDocId);
            query.bindValue(":tid", termId);
            if (!query.exec()) {
                db.rollback();
                QMessageBox::critical(this, "Ошибка БД", "Ошибка связи: " + query.lastError().text());
                return;
            }
        }

        // ---- Статусы терминалов/SIM при редактировании проведённого возврата ----
        QSet<int> checked;
        for (int tid : terminalsToReturn) checked.insert(tid);

        QList<int> newlyReturned;
        for (int tid : checked) {
            if (!m_originalReturned.contains(tid)) newlyReturned.append(tid);
        }
        QList<int> restored;
        for (int tid : m_originalReturned) {
            if (!checked.contains(tid)) restored.append(tid);
        }

        // Терминалы, добавленные в возврат: переводим «в аренде» -> «свободен»
        for (int termId : newlyReturned) {
            QSqlQuery lockTerm(db);
            lockTerm.prepare("SELECT status, currentsimcardid FROM tblterminals WHERE terminalid = :id FOR UPDATE NOWAIT");
            lockTerm.bindValue(":id", termId);
            if (!lockTerm.exec() || !lockTerm.next()) {
                db.rollback();
                QMessageBox::critical(this, "Ошибка",
                    QString("Не удалось заблокировать терминал %1. Возможно, он уже обрабатывается.").arg(termId));
                return;
            }
            if (lockTerm.value(0).toInt() != 1) {
                db.rollback();
                QMessageBox::critical(this, "Ошибка",
                    QString("Терминал %1 уже не находится в аренде!").arg(termId));
                return;
            }
            int simId = lockTerm.value(1).toInt();

            QSqlQuery upd(db);
            upd.prepare("UPDATE tblterminals SET status = 0, currentsimcardid = NULL WHERE terminalid = :id");
            upd.bindValue(":id", termId);
            if (!upd.exec()) {
                db.rollback();
                QMessageBox::critical(this, "Ошибка БД",
                    QString("Не удалось обновить статус терминала %1:\n%2").arg(termId).arg(upd.lastError().text()));
                return;
            }
            if (simId > 0) {
                QSqlQuery updSim(db);
                updSim.prepare("UPDATE tblsimcards SET status = 0 WHERE simcardid = :id");
                updSim.bindValue(":id", simId);
                if (!updSim.exec()) {
                    db.rollback();
                    QMessageBox::critical(this, "Ошибка БД",
                        QString("Не удалось обновить статус SIM-карты %1:\n%2").arg(simId).arg(updSim.lastError().text()));
                    return;
                }
            }
        }

        // Терминалы, убранные из возврата: возвращаем в аренду (с прежней SIM)
        for (int termId : restored) {
            QSqlQuery lockTerm(db);
            lockTerm.prepare("SELECT status FROM tblterminals WHERE terminalid = :id FOR UPDATE NOWAIT");
            lockTerm.bindValue(":id", termId);
            if (!lockTerm.exec() || !lockTerm.next()) {
                db.rollback();
                QMessageBox::critical(this, "Ошибка",
                    QString("Не удалось заблокировать терминал %1. Возможно, он уже обрабатывается.").arg(termId));
                return;
            }
            if (lockTerm.value(0).toInt() != 0) {
                db.rollback();
                QMessageBox::critical(this, "Ошибка",
                    QString("Терминал %1 уже находится в аренде!").arg(termId));
                return;
            }

            // Прежняя SIM-карта из документа аренды
            QSqlQuery origSim(db);
            origSim.prepare("SELECT simcardid FROM tblrentaldetails WHERE rentaldocid = :rd AND terminalid = :tid LIMIT 1");
            origSim.bindValue(":rd", m_editRentalDocId);
            origSim.bindValue(":tid", termId);
            int simId = 0;
            if (origSim.exec() && origSim.next()) {
                simId = origSim.value(0).toInt();
            }

            if (simId > 0) {
                QSqlQuery lockSim(db);
                lockSim.prepare("SELECT status FROM tblsimcards WHERE simcardid = :id FOR UPDATE NOWAIT");
                lockSim.bindValue(":id", simId);
                if (!lockSim.exec() || !lockSim.next()) {
                    db.rollback();
                    QMessageBox::critical(this, "Ошибка",
                        QString("Не удалось заблокировать SIM-карту %1. Возможно, она уже обрабатывается.").arg(simId));
                    return;
                }
                if (lockSim.value(0).toInt() != 0) {
                    db.rollback();
                    QMessageBox::critical(this, "Ошибка",
                        QString("SIM-карта %1 занята, не удалось восстановить аренду терминала %2.").arg(simId).arg(termId));
                    return;
                }
                QSqlQuery updSim(db);
                updSim.prepare("UPDATE tblsimcards SET status = 1 WHERE simcardid = :id");
                updSim.bindValue(":id", simId);
                if (!updSim.exec()) {
                    db.rollback();
                    QMessageBox::critical(this, "Ошибка БД",
                        QString("Не удалось обновить статус SIM-карты %1:\n%2").arg(simId).arg(updSim.lastError().text()));
                    return;
                }
            }

            QSqlQuery upd(db);
            upd.prepare("UPDATE tblterminals SET status = 1, currentsimcardid = :sid WHERE terminalid = :tid");
            upd.bindValue(":sid", simId > 0 ? QVariant(simId) : QVariant());
            upd.bindValue(":tid", termId);
            if (!upd.exec()) {
                db.rollback();
                QMessageBox::critical(this, "Ошибка БД",
                    QString("Не удалось восстановить терминал %1:\n%2").arg(termId).arg(upd.lastError().text()));
                return;
            }
        }

        if (!db.commit()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка", "Не удалось зафиксировать транзакцию");
        } else {
            DatabaseManager::instance().logAction("UPDATE", "tblreturndocs", m_editDocId);
            QMessageBox::information(this, "Успех", "Возврат успешно обновлен!");
            DatabaseManager::instance().notifyDataChanged();
            this->close();
        }
        return;
    }

    // 1. Создаем шапку документа возврата
    if (ui->lineEditNumber->text().trimmed().isEmpty()) {
        QString num = DatabaseManager::instance().generateDocNumber("return");
        if (num.isEmpty()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД", "Не удалось сгенерировать номер документа.");
            return;
        }
        ui->lineEditNumber->setText(num);
    }
    query.prepare("INSERT INTO tblreturndocs (docnumber, docdate, clientid, comments) "
                  "VALUES (:num, :date, :client, :comm) RETURNING returndocid");
    query.bindValue(":num", ui->lineEditNumber->text());
    query.bindValue(":date", QDateTime(ui->dateEdit->date(), QTime::currentTime()));
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
        // Блокируем терминал и проверяем, что он всё ещё в аренде
        QSqlQuery checkQuery(db);
        checkQuery.prepare(
            "SELECT status, currentsimcardid FROM tblterminals "
            "WHERE terminalid = :id FOR UPDATE NOWAIT");
        checkQuery.bindValue(":id", termId);

        if (!checkQuery.exec() || !checkQuery.next()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка",
                QString("Не удалось заблокировать терминал %1. Возможно, он уже обрабатывается.")
                    .arg(termId));
            return;
        }

        int status = checkQuery.value(0).toInt();
        int actualSimId = checkQuery.value(1).toInt(); // читаем SIM ДО очистки

        if (status != 1) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка",
                QString("Терминал %1 уже не находится в аренде!").arg(termId));
            return;
        }

        // Меняем статус терминала на «Свободен» и очищаем привязку SIM
        QSqlQuery updateQuery(db);
        updateQuery.prepare(
            "UPDATE tblterminals SET status = 0, currentsimcardid = NULL "
            "WHERE terminalid = :id");
        updateQuery.bindValue(":id", termId);

        if (!updateQuery.exec()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД",
                QString("Не удалось обновить статус терминала %1:\n%2")
                    .arg(termId).arg(updateQuery.lastError().text()));
            return;
        }

        // Сбрасываем статус SIM-карты (если была привязана)
        if (actualSimId > 0) {
            QSqlQuery simUpdateQuery(db);
            simUpdateQuery.prepare("UPDATE tblsimcards SET status = 0 WHERE simcardid = :id");
            simUpdateQuery.bindValue(":id", actualSimId);

            if (!simUpdateQuery.exec()) {
                db.rollback();
                QMessageBox::critical(this, "Ошибка БД",
                    QString("Не удалось обновить статус SIM-карты %1:\n%2")
                        .arg(actualSimId).arg(simUpdateQuery.lastError().text()));
                return;
            }
        }

        // Записываем в детали возврата
        QSqlQuery detailQuery(db);
        detailQuery.prepare(
            "INSERT INTO tblreturndetails (returndocid, terminalid) "
            "VALUES (:did, :tid)");
        detailQuery.bindValue(":did", docId);
        detailQuery.bindValue(":tid", termId);

        if (!detailQuery.exec()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД",
                "Ошибка связи: " + detailQuery.lastError().text());
            return;
        }
    }

    // 3. Фиксируем транзакцию
    if (!db.commit()) {
        db.rollback();
        QMessageBox::critical(this, "Ошибка", "Не удалось зафиксировать транзакцию");
    } else {
        // Логирование действия
        DatabaseManager::instance().logAction("POST", "tblreturndocs", docId);
        
        QMessageBox::information(this, "Успех", "Возврат успешно проведен!");
        DatabaseManager::instance().notifyDataChanged();
        this->close();
    }
}

void ReturnForm::on_btnPrint_clicked()
{
    int clientId = ui->comboBoxClient->currentData().toInt();
    if (clientId == 0) {
        QMessageBox::warning(this, "Внимание", "Сначала выберите клиента!");
        return;
    }

    if (!m_editMode && ui->lineEditNumber->text().trimmed().isEmpty()) {
        QString num = DatabaseManager::instance().generateDocNumber("return");
        if (num.isEmpty()) {
            QMessageBox::critical(this, "Ошибка БД", "Не удалось сгенерировать номер документа.");
            return;
        }
        ui->lineEditNumber->setText(num);
    }

    QString html = "<html><head><meta charset='utf-8'>"
                   "<style>"
                   "body { font-family: 'Times New Roman', serif; font-size: 14px; }"
                   "h2 { text-align: center; }"
                   "table { border-collapse: collapse; width: 100%; margin-top: 20px; }"
                   "th, td { border: 1px solid black; padding: 6px; text-align: left; }"
                   "th { background-color: #f0f0f0; }"
                   "</style></head><body>";

    html += "<h2>АКТ ВОЗВРАТА ТЕРМИНАЛОВ № " + ui->lineEditNumber->text().toHtmlEscaped() + "</h2>";
    html += "<p>от " + ui->dateEdit->date().toString("dd.MM.yyyy") + " г.</p>";
    html += "<p><b>Арендодатель:</b> ООО «POC Terminal»</p>";

    QSqlQuery clientQuery(DatabaseManager::instance().getDatabase());
    clientQuery.prepare("SELECT clientname, inn FROM tblclients WHERE clientid = :id");
    clientQuery.bindValue(":id", clientId);
    QString clientName, clientInn;
    if (clientQuery.exec() && clientQuery.next()) {
        clientName = clientQuery.value(0).toString();
        clientInn = clientQuery.value(1).toString();
    }
    html += "<p><b>Арендатор:</b> " + clientName.toHtmlEscaped();
    if (!clientInn.isEmpty()) html += " (ИНН: " + clientInn.toHtmlEscaped() + ")";
    html += "</p>";

    html += "<table><tr><th>№</th><th>Серийный номер</th><th>Модель</th><th>IMEI 1</th></tr>";

    for (int i = 0; i < rowsModel->rowCount(); ++i) {
        auto *item = rowsModel->item(i, 0);
        if (!item || item->data(Qt::UserRole).toInt() == 0) continue;
        int termId = item->data(Qt::UserRole).toInt();
        QString serial = item->text();

        QSqlQuery termQuery(DatabaseManager::instance().getDatabase());
        termQuery.prepare("SELECT imei1, COALESCE(m.modelname, '—') FROM tblterminals t "
                          "LEFT JOIN tblmodels m ON t.modelid = m.modelid WHERE t.terminalid = :id");
        termQuery.bindValue(":id", termId);
        QString imei, modelName;
        if (termQuery.exec() && termQuery.next()) {
            imei = termQuery.value(0).toString();
            modelName = termQuery.value(1).toString();
        }

        html += "<tr><td>" + QString::number(i + 1) + "</td>"
                "<td>" + serial.toHtmlEscaped() + "</td>"
                "<td>" + modelName.toHtmlEscaped() + "</td>"
                "<td>" + imei.toHtmlEscaped() + "</td></tr>";
    }
    html += "</table>";

    html += "<div style='margin-top: 40px; display: flex; justify-content: space-between;'>"
            "<div><p>Сдал (Арендатор):</p><p>________________ / ____________</p></div>"
            "<div><p>Принял (Арендодатель):</p><p>________________ / ____________</p></div>"
            "</div>";
    html += "</body></html>";

    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog printDialog(&printer, this);
    if (printDialog.exec() == QDialog::Accepted) {
        QTextDocument doc;
        doc.setHtml(html);
        doc.print(&printer);
    }
}

void ReturnForm::on_btnClose_clicked()
{
    close();
}
