#include "returnform.h"
#include "ui_returnform.h"
#include "database/databasemanager.h"
#include "database/repositories/clientrepository.h"
#include "database/repositories/documentrepository.h"
#include "database/repositories/terminalrepository.h"
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QTime>
#include <QDebug>
#include <QPrinter>
#include "utils/logging.h"
#include "services/documentnumbergenerator.h"
#include "services/postactionlogger.h"
#include "ui/base/printservice.h"
#include "ui/base/transactionguard.h"
#include <QHash>

ReturnForm::ReturnForm(QWidget* parent) : DocumentDialog(parent), ui(new Ui::ReturnForm)
{
    ui->setupUi(this);
    setWindowTitle("Документ: Возврат из аренды");
    resize(900, 600);

    ui->dateEdit->setDate(QDate::currentDate());

    // Номер документа генерируется при проведении (не здесь), чтобы не
    // сжигать значения последовательности для отменённых форм.

    // Настройка модели таблицы
    rowsModel->setColumnCount(4);
    rowsModel->setHorizontalHeaderLabels({"Возврат", "Терминал", "SIM (IMEI 1)", "SIM (IMEI 2)"});
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

QString ReturnForm::docType() const
{
    return "return";
}

QLineEdit* ReturnForm::headerNumberEdit() const
{
    return ui->lineEditNumber;
}

QDateEdit* ReturnForm::headerDateEdit() const
{
    return ui->dateEdit;
}

QTextEdit* ReturnForm::headerCommentEdit() const
{
    return ui->textEditComment;
}

QTableView* ReturnForm::tableView() const
{
    return ui->tableView;
}


void ReturnForm::loadClientsToComboBox()
{
    const auto clients = ClientRepository(DatabaseManager::instance().getDatabase()).loadAll();
    for (const auto& c : clients)
        ui->comboBoxClient->addItem(c.name, c.id);
}

void ReturnForm::loadRentalDocs(int clientId)
{
    // Очищаем ComboBox документов
    ui->comboBoxRentalDoc->clear();
    rowsModel->removeRows(0, rowsModel->rowCount());

    if (clientId == 0)
        return;

    // Загружаем документы аренды для этого клиента
    const auto docs =
        DocumentRepository(DatabaseManager::instance().getDatabase()).loadRentalDocumentsByClient(clientId);
    for (const auto& d : docs) {
        QString displayText = QString("%1 от %2").arg(d.docNumber, d.date.toString("dd.MM.yyyy"));
        ui->comboBoxRentalDoc->addItem(displayText, d.id);
    }
}

void ReturnForm::loadRentalDetails(int rentalDocId)
{
    rowsModel->removeRows(0, rowsModel->rowCount());
    if (rentalDocId == 0)
        return;

    // Загружаем строки из документа аренды
    // Показываем все терминалы из документа, даже если они уже возвращены (для истории)
    const auto rows = DocumentRepository(DatabaseManager::instance().getDatabase()).loadRentalRows(rentalDocId);
    for (const auto& row : rows) {
        int r = rowsModel->rowCount();
        rowsModel->insertRow(r);

        // Колонка 0: Чекбокс "Возврат"
        QStandardItem* checkItem = new QStandardItem();
        checkItem->setCheckable(true);
        checkItem->setCheckState(Qt::Unchecked);
        checkItem->setData(row.terminalId, Qt::UserRole); // Храним ID терминала
        rowsModel->setItem(r, 0, checkItem);

        // Колонка 1: Терминал (только для чтения)
        QStandardItem* termItem = new QStandardItem(row.terminalSerialNumber);
        termItem->setEditable(false);
        termItem->setData(row.terminalId, Qt::UserRole);
        rowsModel->setItem(r, 1, termItem);

        // Колонка 2: SIM слота 1 (только для чтения)
        QString simNumber = row.simCardId > 0 ? row.simNumber : QString("Нет SIM");
        QStandardItem* simItem = new QStandardItem(simNumber);
        simItem->setEditable(false);
        rowsModel->setItem(r, 2, simItem);

        // Колонка 3: SIM слота 2 (только для чтения)
        QString sim2Number = row.simCard2Id > 0 ? row.simNumber2 : QString("Нет SIM");
        QStandardItem* sim2Item = new QStandardItem(sim2Number);
        sim2Item->setEditable(false);
        rowsModel->setItem(r, 3, sim2Item);
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

void ReturnForm::loadSpecificEditData(int docId)
{
    m_originalReturned.clear();
    m_editRentalDocId = 0;

    const QSqlDatabase& db = DatabaseManager::instance().getDatabase();
    DocumentRepository documents(db);

    const models::DocumentHeader header = documents.loadHeader(DocumentRepository::Return, docId);
    if (header.id == 0) {
        QMessageBox::critical(this, "Ошибка", "Не удалось загрузить документ возврата");
        return;
    }

    ui->lineEditNumber->setText(header.docNumber);
    ui->lineEditNumber->setReadOnly(true);
    ui->dateEdit->setDate(header.date);
    ui->textEditComment->setText(header.comments);
    setWindowTitle(QString("Редактирование возврата ID %1").arg(docId));

    int clientIndex = ui->comboBoxClient->findData(header.clientId);
    if (clientIndex >= 0) {
        ui->comboBoxClient->setCurrentIndex(clientIndex);
    }

    int rentalDocId = documents.rentalDocIdForReturn(docId);
    if (rentalDocId > 0) {
        m_editRentalDocId = rentalDocId;
        int rentalIndex = ui->comboBoxRentalDoc->findData(rentalDocId);
        if (rentalIndex >= 0) {
            ui->comboBoxRentalDoc->setCurrentIndex(rentalIndex);
        }
    }

    const QList<int> returnedTerminals = documents.returnedTerminalIds(docId);
    for (int tid : returnedTerminals)
        m_originalReturned.insert(tid);

    for (int i = 0; i < rowsModel->rowCount(); ++i) {
        QStandardItem* checkItem = rowsModel->item(i, 0);
        if (checkItem && returnedTerminals.contains(checkItem->data(Qt::UserRole).toInt())) {
            checkItem->setCheckState(Qt::Checked);
        }
    }
}

void ReturnForm::on_btnPost_clicked()
{
    executePost();
}

bool ReturnForm::validateBeforePost()
{
    int clientId = ui->comboBoxClient->currentData().toInt();
    int rentalDocId = ui->comboBoxRentalDoc->currentData().toInt();

    m_terminalsToReturn.clear();

    if (clientId == 0) {
        QMessageBox::warning(this, "Внимание", "Выберите клиента!");
        return false;
    }
    if (rentalDocId == 0) {
        QMessageBox::warning(this, "Внимание", "Выберите документ аренды!");
        return false;
    }

    // Собираем ID терминалов, которые нужно вернуть
    QList<int> m_terminalsToReturn;
    for (int i = 0; i < rowsModel->rowCount(); ++i) {
        QStandardItem* checkItem = rowsModel->item(i, 0);
        if (checkItem && checkItem->checkState() == Qt::Checked) {
            m_terminalsToReturn.append(checkItem->data(Qt::UserRole).toInt());
        }
    }

    if (m_terminalsToReturn.isEmpty()) {
        QMessageBox::warning(this, "Внимание", "Отметьте хотя бы один терминал для возврата!");
        return false;
    }
    return true;
}

int ReturnForm::postHeader(QSqlDatabase& db)
{
    int clientId = ui->comboBoxClient->currentData().toInt();
    QSqlQuery query(db);

    if (m_editMode) {
        // Режим редактирования: обновляем шапку и детали
        query.prepare(
            "UPDATE tblreturndocs SET docdate = :date, clientid = :client, comments = :comm WHERE returndocid = :id");
        query.bindValue(":date", QDateTime(ui->dateEdit->date(), QTime::currentTime()));
        query.bindValue(":client", clientId);
        query.bindValue(":comm", ui->textEditComment->toPlainText());
        query.bindValue(":id", m_editDocId);

        if (!query.exec()) {
            QMessageBox::critical(this, "Ошибка БД", "Не удалось обновить шапку: " + query.lastError().text());
            return -1;
        }

        query.prepare("DELETE FROM tblreturndetails WHERE returndocid = :id");
        query.bindValue(":id", m_editDocId);
        if (!query.exec()) {
            QMessageBox::critical(this, "Ошибка БД", "Не удалось удалить детали: " + query.lastError().text());
            return -1;
        }

        for (int termId : m_terminalsToReturn) {
            query.prepare("INSERT INTO tblreturndetails (returndocid, terminalid) VALUES (:did, :tid)");
            query.bindValue(":did", m_editDocId);
            query.bindValue(":tid", termId);
            if (!query.exec()) {
                QMessageBox::critical(this, "Ошибка БД", "Ошибка связи: " + query.lastError().text());
                return -1;
            }
        }
        return m_editDocId;
    }

    // 1. Создаем шапку документа возврата
    if (ui->lineEditNumber->text().trimmed().isEmpty()) {
        QString num = DocumentNumberGenerator::generate("return", db);
        if (num.isEmpty()) {
            QMessageBox::critical(this, "Ошибка БД", "Не удалось сгенерировать номер документа.");
            return -1;
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
        QMessageBox::critical(this, "Ошибка БД", "Не удалось создать шапку: " + query.lastError().text());
        return -1;
    }
    int docId = query.value(0).toInt();
}

bool ReturnForm::postDetails(QSqlDatabase& db, int docId)
{
    if (m_editMode) {
        // ---- Статусы терминалов/SIM при редактировании проведённого возврата ----
        QSet<int> checked;
        for (int tid : m_terminalsToReturn)
            checked.insert(tid);

        QList<int> newlyReturned;
        for (int tid : checked) {
            if (!m_originalReturned.contains(tid))
                newlyReturned.append(tid);
        }
        QList<int> restored;
        for (int tid : m_originalReturned) {
            if (!checked.contains(tid))
                restored.append(tid);
        }

        // Терминалы, добавленные в возврат: переводим «в аренде» -> «свободен»
        for (int termId : newlyReturned) {
            QSqlQuery lockTerm(db);
            lockTerm.prepare("SELECT status, currentsimcardid, currentsimcardid2 "
                             "FROM tblterminals WHERE terminalid = :id FOR UPDATE NOWAIT");
            lockTerm.bindValue(":id", termId);
            if (!lockTerm.exec() || !lockTerm.next()) {
                QMessageBox::critical(
                    this, "Ошибка",
                    QString("Не удалось заблокировать терминал %1. Возможно, он уже обрабатывается.").arg(termId));
                return false;
            }
            if (lockTerm.value(0).toInt() != 1) {
                QMessageBox::critical(this, "Ошибка", QString("Терминал %1 уже не находится в аренде!").arg(termId));
                return false;
            }
            int simId = lockTerm.value(1).toInt();  // SIM слота 1 (imei1)
            int sim2Id = lockTerm.value(2).toInt(); // SIM слота 2 (imei2)

            QSqlQuery upd(db);
            upd.prepare("UPDATE tblterminals SET status = 0, currentsimcardid = NULL, currentsimcardid2 = NULL "
                        "WHERE terminalid = :id");
            upd.bindValue(":id", termId);
            if (!upd.exec()) {
                QMessageBox::critical(
                    this, "Ошибка БД",
                    QString("Не удалось обновить статус терминала %1:\n%2").arg(termId).arg(upd.lastError().text()));
                return false;
            }
            for (int sid : {simId, sim2Id}) {
                if (sid <= 0)
                    continue;
                QSqlQuery updSim(db);
                updSim.prepare("UPDATE tblsimcards SET status = 0 WHERE simcardid = :id");
                updSim.bindValue(":id", sid);
                if (!updSim.exec()) {
                    QMessageBox::critical(this, "Ошибка БД",
                                          QString("Не удалось обновить статус SIM-карты %1:\n%2")
                                              .arg(sid)
                                              .arg(updSim.lastError().text()));
                    return false;
                }
            }
        }

        // Терминалы, убранные из возврата: возвращаем в аренду (с прежней SIM)
        for (int termId : restored) {
            QSqlQuery lockTerm(db);
            lockTerm.prepare("SELECT status FROM tblterminals WHERE terminalid = :id FOR UPDATE NOWAIT");
            lockTerm.bindValue(":id", termId);
            if (!lockTerm.exec() || !lockTerm.next()) {
                QMessageBox::critical(
                    this, "Ошибка",
                    QString("Не удалось заблокировать терминал %1. Возможно, он уже обрабатывается.").arg(termId));
                return false;
            }
            if (lockTerm.value(0).toInt() != 0) {
                QMessageBox::critical(this, "Ошибка", QString("Терминал %1 уже находится в аренде!").arg(termId));
                return false;
            }

            // Прежние SIM-карты из документа аренды (слот 1 и слот 2)
            QSqlQuery origSim(db);
            origSim.prepare("SELECT simcardid, simcardid2 FROM tblrentaldetails "
                            "WHERE rentaldocid = :rd AND terminalid = :tid LIMIT 1");
            origSim.bindValue(":rd", m_editRentalDocId);
            origSim.bindValue(":tid", termId);
            int simId = 0;
            int sim2Id = 0;
            if (origSim.exec() && origSim.next()) {
                simId = origSim.value(0).toInt();
                sim2Id = origSim.value(1).toInt();
            }

            for (int sid : {simId, sim2Id}) {
                if (sid <= 0)
                    continue;
                QSqlQuery lockSim(db);
                lockSim.prepare("SELECT status FROM tblsimcards WHERE simcardid = :id FOR UPDATE NOWAIT");
                lockSim.bindValue(":id", sid);
                if (!lockSim.exec() || !lockSim.next()) {
                    QMessageBox::critical(
                        this, "Ошибка",
                        QString("Не удалось заблокировать SIM-карту %1. Возможно, она уже обрабатывается.").arg(sid));
                    return false;
                }
                if (lockSim.value(0).toInt() != 0) {
                    QMessageBox::critical(this, "Ошибка",
                                          QString("SIM-карта %1 занята, не удалось восстановить аренду терминала %2.")
                                              .arg(sid)
                                              .arg(termId));
                    return false;
                }
                QSqlQuery updSim(db);
                updSim.prepare("UPDATE tblsimcards SET status = 1 WHERE simcardid = :id");
                updSim.bindValue(":id", sid);
                if (!updSim.exec()) {
                    QMessageBox::critical(this, "Ошибка БД",
                                          QString("Не удалось обновить статус SIM-карты %1:\n%2")
                                              .arg(sid)
                                              .arg(updSim.lastError().text()));
                    return false;
                }
            }

            QSqlQuery upd(db);
            upd.prepare("UPDATE tblterminals SET status = 1, currentsimcardid = :sid, "
                        "currentsimcardid2 = :sid2 WHERE terminalid = :tid");
            upd.bindValue(":sid", simId > 0 ? QVariant(simId) : QVariant());
            upd.bindValue(":sid2", sim2Id > 0 ? QVariant(sim2Id) : QVariant());
            upd.bindValue(":tid", termId);
            if (!upd.exec()) {
                QMessageBox::critical(
                    this, "Ошибка БД",
                    QString("Не удалось восстановить терминал %1:\n%2").arg(termId).arg(upd.lastError().text()));
                return false;
            }
        }
    } else {

    // 2. Обрабатываем выбранные терминалы
    for (int termId : m_terminalsToReturn) {
        // Блокируем терминал и проверяем, что он всё ещё в аренде
        QSqlQuery checkQuery(db);
        checkQuery.prepare("SELECT status, currentsimcardid, currentsimcardid2 FROM tblterminals "
                           "WHERE terminalid = :id FOR UPDATE NOWAIT");
        checkQuery.bindValue(":id", termId);

        if (!checkQuery.exec() || !checkQuery.next()) {
            QMessageBox::critical(
                this, "Ошибка",
                QString("Не удалось заблокировать терминал %1. Возможно, он уже обрабатывается.").arg(termId));
            return false;
        }

        int status = checkQuery.value(0).toInt();
        int actualSimId = checkQuery.value(1).toInt();  // SIM слота 1 (imei1)
        int actualSim2Id = checkQuery.value(2).toInt(); // SIM слота 2 (imei2)

        if (status != 1) {
            QMessageBox::critical(this, "Ошибка", QString("Терминал %1 уже не находится в аренде!").arg(termId));
            return false;
        }

        // Меняем статус терминала на «Свободен» и очищаем привязки SIM
        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE tblterminals SET status = 0, currentsimcardid = NULL, "
                            "currentsimcardid2 = NULL WHERE terminalid = :id");
        updateQuery.bindValue(":id", termId);

        if (!updateQuery.exec()) {
            QMessageBox::critical(this, "Ошибка БД",
                                  QString("Не удалось обновить статус терминала %1:\n%2")
                                      .arg(termId)
                                      .arg(updateQuery.lastError().text()));
            return false;
        }

        // Сбрасываем статусы SIM-карт (если были привязаны)
        for (int simId : {actualSimId, actualSim2Id}) {
            if (simId <= 0)
                continue;
            QSqlQuery simUpdateQuery(db);
            simUpdateQuery.prepare("UPDATE tblsimcards SET status = 0 WHERE simcardid = :id");
            simUpdateQuery.bindValue(":id", simId);

            if (!simUpdateQuery.exec()) {
                QMessageBox::critical(this, "Ошибка БД",
                                      QString("Не удалось обновить статус SIM-карты %1:\n%2")
                                          .arg(simId)
                                          .arg(simUpdateQuery.lastError().text()));
                return false;
            }
        }

        // Записываем в детали возврата
        QSqlQuery detailQuery(db);
        detailQuery.prepare("INSERT INTO tblreturndetails (returndocid, terminalid) "
                            "VALUES (:did, :tid)");
        detailQuery.bindValue(":did", docId);
        detailQuery.bindValue(":tid", termId);

        if (!detailQuery.exec()) {
            QMessageBox::critical(this, "Ошибка БД", "Ошибка связи: " + detailQuery.lastError().text());
            return false;
        }
    }
    }
    return true;
}

void ReturnForm::onPostSuccess(int docId)
{
    if (m_editMode) {
        PostActionLogger::log("UPDATE", "tblreturndocs", docId);
        QMessageBox::information(this, "Успех", "Возврат успешно обновлен!");
    } else {
        PostActionLogger::log("POST", "tblreturndocs", docId);
        QMessageBox::information(this, "Успех", "Возврат успешно проведен!");
    }
    PostActionLogger::notify();
    this->close();
}
void ReturnForm::on_btnPrint_clicked()
{
    int clientId = ui->comboBoxClient->currentData().toInt();
    if (clientId == 0) {
        QMessageBox::warning(this, "Внимание", "Сначала выберите клиента!");
        return;
    }

    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    if (!m_editMode && ui->lineEditNumber->text().trimmed().isEmpty()) {
        QString num = DocumentNumberGenerator::generate("return", db);
        if (num.isEmpty()) {
            QMessageBox::critical(this, "Ошибка БД", "Не удалось сгенерировать номер документа.");
            return;
        }
        ui->lineEditNumber->setText(num);
    }

    QString html = PrintService::docHeader();

    html += "<h2>АКТ ВОЗВРАТА ТЕРМИНАЛОВ № " + ui->lineEditNumber->text().toHtmlEscaped() + "</h2>";
    html += "<p>от " + ui->dateEdit->date().toString("dd.MM.yyyy") + " г.</p>";
    html += "<p><b>Арендодатель:</b> ООО «POC Terminal»</p>";

    const models::Client client = ClientRepository(db).loadById(clientId);
    QString clientName = client.name;
    QString clientInn = client.inn;
    html += "<p><b>Арендатор:</b> " + clientName.toHtmlEscaped();
    if (!clientInn.isEmpty())
        html += " (ИНН: " + clientInn.toHtmlEscaped() + ")";
    html += "</p>";

    html += "<table><tr><th>№</th><th>Серийный номер</th><th>Модель</th><th>IMEI 1</th></tr>";

    QList<int> terminalIds;
    for (int i = 0; i < rowsModel->rowCount(); ++i) {
        auto* item = rowsModel->item(i, 0);
        if (item && item->data(Qt::UserRole).toInt() > 0)
            terminalIds.append(item->data(Qt::UserRole).toInt());
    }
    QHash<int, models::Terminal> termById;
    for (const auto& t : TerminalRepository(db).loadByIds(terminalIds))
        termById.insert(t.id, t);

    for (int i = 0; i < rowsModel->rowCount(); ++i) {
        auto* item = rowsModel->item(i, 0);
        if (!item || item->data(Qt::UserRole).toInt() == 0)
            continue;
        int termId = item->data(Qt::UserRole).toInt();
        QString serial = item->text();

        const models::Terminal term = termById.value(termId);
        QString imei = term.imei1;
        QString modelName = term.modelName.isEmpty() ? QString("—") : term.modelName;

        html += "<tr><td>" + QString::number(i + 1) +
                "</td>"
                "<td>" +
                serial.toHtmlEscaped() +
                "</td>"
                "<td>" +
                modelName.toHtmlEscaped() +
                "</td>"
                "<td>" +
                imei.toHtmlEscaped() + "</td></tr>";
    }
    html += "</table>";

    html += "<div style='margin-top: 40px; display: flex; justify-content: space-between;'>"
            "<div><p>Сдал (Арендатор):</p><p>________________ / ____________</p></div>"
            "<div><p>Принял (Арендодатель):</p><p>________________ / ____________</p></div>"
            "</div>";
    html += PrintService::docFooter();

    PrintService::printHtml(html, this);
}

void ReturnForm::on_btnClose_clicked()
{
    close();
}
