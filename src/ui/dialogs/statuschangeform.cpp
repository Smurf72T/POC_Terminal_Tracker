#include "statuschangeform.h"
#include "ui_statuschangeform.h"
#include "database/databasemanager.h"
#include "utils/logging.h"
#include "utils/terminal_status.h"
#include "services/documentnumbergenerator.h"
#include "services/postactionlogger.h"
#include "ui/base/printservice.h"
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QTime>
#include <QDebug>

StatusChangeForm::StatusChangeForm(QWidget* parent) :
    QDialog(parent), ui(new Ui::StatusChangeForm), rowsModel(new QStandardItemModel(0, 6, this))
{
    ui->setupUi(this);
    setWindowTitle("Документ: Изменение статуса терминалов");
    resize(900, 620);

    ui->comboBoxActionType->setItemData(0, "repair");
    ui->comboBoxActionType->setItemData(1, "repair_return");
    ui->comboBoxActionType->setItemData(2, "writeoff");
    ui->comboBoxActionType->setItemData(3, "lost");

    ui->dateEdit->setDate(QDate::currentDate());

    // Номер документа генерируется при проведении (не здесь), чтобы не
    // сжигать значения последовательности для отменённых форм.

    rowsModel->setHorizontalHeaderLabels({"Выбрать", "Серийный номер", "Модель", "IMEI 1", "Статус", "Был в ремонте"});
    ui->tableView->setModel(rowsModel);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setAlternatingRowColors(true);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);

    loadRepairDocs();
    updateWindowTitle();
    loadTerminals();
}

StatusChangeForm::~StatusChangeForm()
{
    delete ui;
}

QString StatusChangeForm::actionType() const
{
    return ui->comboBoxActionType->currentData().toString();
}

QString StatusChangeForm::actionTitle() const
{
    if (actionType() == "repair")
        return "В ремонт";
    if (actionType() == "repair_return")
        return "Возврат из ремонта";
    if (actionType() == "writeoff")
        return "Списан";
    if (actionType() == "lost")
        return "Утерян";
    return "Изменение статуса";
}

void StatusChangeForm::updateWindowTitle()
{
    setWindowTitle(QString("Документ: %1").arg(actionTitle()));
}

int StatusChangeForm::targetStatus() const
{
    if (actionType() == "repair")
        return 2;
    if (actionType() == "repair_return")
        return 0;
    if (actionType() == "writeoff")
        return 3;
    if (actionType() == "lost")
        return 4;
    return 0;
}

bool StatusChangeForm::expectStatus(int status) const
{
    if (actionType() == "repair")
        return status == 0;
    if (actionType() == "repair_return")
        return status == 2;
    if (actionType() == "writeoff")
        return status == 0 || status == 2;
    if (actionType() == "lost")
        return status == 1 || status == 2;
    return false;
}

QString StatusChangeForm::statusText(int status) const
{
    return TerminalStatus::name(status);
}

void StatusChangeForm::loadRepairDocs()
{
    ui->comboBoxRepairDoc->clear();
    ui->comboBoxRepairDoc->addItem("Все терминалы в ремонте", 0);

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("SELECT statuschangedocid, docnumber, docdate FROM tblstatuschangedocs "
                  "WHERE actiontype = 'repair' ORDER BY docdate DESC");
    if (!query.exec()) {
        qCWarning(logSQL) << "Failed to load repair docs:" << query.lastError().text();
        return;
    }

    while (query.next()) {
        QString display =
            QString("%1 от %2").arg(query.value(1).toString(), query.value(2).toDateTime().toString("dd.MM.yyyy"));
        ui->comboBoxRepairDoc->addItem(display, query.value(0).toInt());
    }
}

void StatusChangeForm::loadTerminalsFromRepairDoc(int repairDocId)
{
    rowsModel->removeRows(0, rowsModel->rowCount());
    if (repairDocId == 0) {
        loadTerminals();
        return;
    }

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("SELECT t.terminalid, t.serialnumber, "
                  "COALESCE(m.modelname, '—') AS modelname, "
                  "COALESCE(t.imei1, '') AS imei1, " +
                  TerminalStatus::sqlCaseExpression("t.status") +
                  " AS status, "
                  "t.was_repaired "
                  "FROM tblstatuschangedetails scd "
                  "JOIN tblterminals t ON scd.terminalid = t.terminalid "
                  "LEFT JOIN tblmodels m ON t.modelid = m.modelid "
                  "WHERE scd.statuschangedocid = :did AND t.status = 2 "
                  "ORDER BY t.serialnumber");
    query.bindValue(":did", repairDocId);

    if (!query.exec()) {
        qCWarning(logSQL) << "Failed to load repair doc terminals:" << query.lastError().text();
        return;
    }

    while (query.next()) {
        int row = rowsModel->rowCount();
        rowsModel->insertRow(row);

        QStandardItem* checkItem = new QStandardItem();
        checkItem->setCheckable(true);
        checkItem->setCheckState(Qt::Unchecked);
        checkItem->setData(query.value(0).toInt(), Qt::UserRole);
        rowsModel->setItem(row, 0, checkItem);

        for (int col = 1; col < 6; ++col) {
            QStandardItem* item = new QStandardItem(query.value(col).toString());
            item->setEditable(false);
            item->setData(query.value(0).toInt(), Qt::UserRole);
            rowsModel->setItem(row, col, item);
        }
    }
}

void StatusChangeForm::loadTerminals()
{
    rowsModel->removeRows(0, rowsModel->rowCount());

    QString statusCond;
    if (actionType() == "repair")
        statusCond = "t.status = 0";
    else if (actionType() == "repair_return")
        statusCond = "t.status = 2";
    else if (actionType() == "writeoff")
        statusCond = "t.status IN (0, 2)";
    else if (actionType() == "lost")
        statusCond = "t.status IN (1, 2)";
    else
        return;

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("SELECT t.terminalid, t.serialnumber, "
                  "COALESCE(m.modelname, '—') AS modelname, "
                  "COALESCE(t.imei1, '') AS imei1, " +
                  TerminalStatus::sqlCaseExpression("t.status") +
                  " AS status, "
                  "t.was_repaired "
                  "FROM tblterminals t "
                  "LEFT JOIN tblmodels m ON t.modelid = m.modelid "
                  "WHERE " +
                  statusCond +
                  " "
                  "ORDER BY t.serialnumber");

    if (!query.exec()) {
        qCWarning(logSQL) << "Failed to load terminals:" << query.lastError().text();
        return;
    }

    while (query.next()) {
        int row = rowsModel->rowCount();
        rowsModel->insertRow(row);

        QStandardItem* checkItem = new QStandardItem();
        checkItem->setCheckable(true);
        checkItem->setCheckState(Qt::Unchecked);
        checkItem->setData(query.value(0).toInt(), Qt::UserRole);
        rowsModel->setItem(row, 0, checkItem);

        for (int col = 1; col < 6; ++col) {
            QStandardItem* item = new QStandardItem(query.value(col).toString());
            item->setEditable(false);
            item->setData(query.value(0).toInt(), Qt::UserRole);
            rowsModel->setItem(row, col, item);
        }
    }
}

void StatusChangeForm::on_comboBoxActionType_currentIndexChanged(int index)
{
    Q_UNUSED(index);
    bool isReturn = actionType() == "repair_return";
    ui->labelRepairDoc->setVisible(isReturn);
    ui->comboBoxRepairDoc->setVisible(isReturn);
    updateWindowTitle();
    loadTerminals();
}

void StatusChangeForm::on_comboBoxRepairDoc_currentIndexChanged(int index)
{
    loadTerminalsFromRepairDoc(ui->comboBoxRepairDoc->itemData(index).toInt());
}

void StatusChangeForm::on_btnRefresh_clicked()
{
    if (actionType() == "repair_return" && ui->comboBoxRepairDoc->currentData().toInt() > 0) {
        loadTerminalsFromRepairDoc(ui->comboBoxRepairDoc->currentData().toInt());
    } else {
        loadTerminals();
    }
}

QList<int> StatusChangeForm::checkedTerminalIds() const
{
    QList<int> ids;
    for (int i = 0; i < rowsModel->rowCount(); ++i) {
        QStandardItem* checkItem = rowsModel->item(i, 0);
        if (checkItem && checkItem->checkState() == Qt::Checked) {
            ids.append(checkItem->data(Qt::UserRole).toInt());
        }
    }
    return ids;
}

void StatusChangeForm::on_btnPost_clicked()
{
    QString comment = ui->textEditComment->toPlainText().trimmed();
    if (comment.isEmpty()) {
        QMessageBox::warning(this, "Внимание", "Укажите комментарий: что ремонтируется или причина списания/утери.");
        return;
    }

    QList<int> terminalIds = checkedTerminalIds();
    if (terminalIds.isEmpty()) {
        QMessageBox::warning(this, "Внимание", "Отметьте хотя бы один терминал!");
        return;
    }

    int basedocid = (actionType() == "repair_return") ? ui->comboBoxRepairDoc->currentData().toInt() : 0;

    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    if (!db.transaction()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось начать транзакцию");
        return;
    }

    QSqlQuery query(db);
    int docId = m_editDocId;

    if (m_editMode) {
        query.prepare("UPDATE tblstatuschangedocs SET docdate = :date, actiontype = :type, "
                      "comment = :comm, basedocid = :base WHERE statuschangedocid = :id");
        query.bindValue(":date", QDateTime(ui->dateEdit->date(), QTime::currentTime()));
        query.bindValue(":type", actionType());
        query.bindValue(":comm", comment);
        if (basedocid > 0) {
            query.bindValue(":base", basedocid);
        } else {
            query.bindValue(":base", QVariant());
        }
        query.bindValue(":id", m_editDocId);
        if (!query.exec()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД", "Не удалось обновить шапку: " + query.lastError().text());
            return;
        }

        query.prepare("DELETE FROM tblstatuschangedetails WHERE statuschangedocid = :id");
        query.bindValue(":id", m_editDocId);
        if (!query.exec()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД", "Не удалось обновить строки: " + query.lastError().text());
            return;
        }
    } else {
        if (ui->lineEditNumber->text().trimmed().isEmpty()) {
            QString num = DocumentNumberGenerator::generate("statuschange", db);
            if (num.isEmpty()) {
                db.rollback();
                QMessageBox::critical(this, "Ошибка БД", "Не удалось сгенерировать номер документа.");
                return;
            }
            ui->lineEditNumber->setText(num);
        }
        query.prepare("INSERT INTO tblstatuschangedocs (docnumber, docdate, actiontype, comment, basedocid) "
                      "VALUES (:num, :date, :type, :comm, :base) RETURNING statuschangedocid");
        query.bindValue(":num", ui->lineEditNumber->text());
        query.bindValue(":date", QDateTime(ui->dateEdit->date(), QTime::currentTime()));
        query.bindValue(":type", actionType());
        query.bindValue(":comm", comment);
        if (basedocid > 0) {
            query.bindValue(":base", basedocid);
        } else {
            query.bindValue(":base", QVariant());
        }

        if (!query.exec() || !query.next()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД", "Не удалось создать документ: " + query.lastError().text());
            return;
        }
        docId = query.value(0).toInt();
    }

    int target = targetStatus();

    // Терминалы, убранные из документа при редактировании, должны быть
    // возвращены в прежний статус (откат операции).
    QSet<int> checked;
    for (int tid : terminalIds)
        checked.insert(tid);
    QList<int> removed;
    if (m_editMode) {
        for (int tid : m_originalTerminals) {
            if (!checked.contains(tid))
                removed.append(tid);
        }
    }

    for (int termId : terminalIds) {
        QSqlQuery checkQuery(db);
        checkQuery.prepare("SELECT status FROM tblterminals "
                           "WHERE terminalid = :id FOR UPDATE NOWAIT");
        checkQuery.bindValue(":id", termId);

        if (!checkQuery.exec() || !checkQuery.next()) {
            db.rollback();
            QMessageBox::critical(
                this, "Ошибка",
                QString("Не удалось заблокировать терминал %1. Возможно, он уже обрабатывается.").arg(termId));
            return;
        }

        int currentStatus = checkQuery.value(0).toInt();
        bool ok = expectStatus(currentStatus);
        // При редактировании терминал может уже стоять в целевом статусе —
        // это допустимо (повторное применение идемпотентно).
        if (m_editMode)
            ok = ok || (currentStatus == target);
        if (!ok) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка",
                                  QString("Терминал %1 имеет статус «%2», операция «%3» для него недоступна.")
                                      .arg(termId)
                                      .arg(statusText(currentStatus))
                                      .arg(actionTitle()));
            return;
        }

        // Прежний статус: для терминалов из исходного документа берём снимок,
        // для добавленных — текущий статус.
        int oldStatus = currentStatus;
        if (m_editMode && m_originalTerminals.contains(termId))
            oldStatus = m_originalStatus.value(termId, currentStatus);

        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE tblterminals SET status = :status, "
                            "was_repaired = CASE WHEN :status = 2 THEN TRUE ELSE was_repaired END "
                            "WHERE terminalid = :id");
        updateQuery.bindValue(":status", target);
        updateQuery.bindValue(":id", termId);
        if (!updateQuery.exec()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД",
                                  QString("Не удалось обновить статус терминала %1:\n%2")
                                      .arg(termId)
                                      .arg(updateQuery.lastError().text()));
            return;
        }

        QSqlQuery detailQuery(db);
        detailQuery.prepare("INSERT INTO tblstatuschangedetails (statuschangedocid, terminalid, old_status) "
                            "VALUES (:did, :tid, :old)");
        detailQuery.bindValue(":did", docId);
        detailQuery.bindValue(":tid", termId);
        detailQuery.bindValue(":old", oldStatus);
        if (!detailQuery.exec()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД", "Ошибка связи: " + detailQuery.lastError().text());
            return;
        }
    }

    // Откат статусов для терминалов, убранных из документа
    for (int termId : removed) {
        QSqlQuery checkQuery(db);
        checkQuery.prepare("SELECT status FROM tblterminals "
                           "WHERE terminalid = :id FOR UPDATE NOWAIT");
        checkQuery.bindValue(":id", termId);

        if (!checkQuery.exec() || !checkQuery.next()) {
            db.rollback();
            QMessageBox::critical(
                this, "Ошибка",
                QString("Не удалось заблокировать терминал %1. Возможно, он уже обрабатывается.").arg(termId));
            return;
        }

        int currentStatus = checkQuery.value(0).toInt();
        if (currentStatus != target) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка",
                                  QString("Статус терминала %1 («%2») уже изменён другим документом, "
                                          "откат невозможен. Уберите его вручную.")
                                      .arg(termId)
                                      .arg(statusText(currentStatus)));
            return;
        }

        int oldStatus = m_originalStatus.value(termId, -1);
        if (oldStatus < 0) {
            if (m_originalActionType == "repair")
                oldStatus = 0;
            else if (m_originalActionType == "repair_return")
                oldStatus = 2;
            else {
                db.rollback();
                QMessageBox::critical(this, "Ошибка",
                                      QString("Прежний статус терминала %1 неизвестен (документ проведён до "
                                              "введения снимков статусов). Верните его вручную.")
                                          .arg(termId));
                return;
            }
        }

        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE tblterminals SET status = :status WHERE terminalid = :id");
        updateQuery.bindValue(":status", oldStatus);
        updateQuery.bindValue(":id", termId);
        if (!updateQuery.exec()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД",
                                  QString("Не удалось восстановить статус терминала %1:\n%2")
                                      .arg(termId)
                                      .arg(updateQuery.lastError().text()));
            return;
        }
    }

    if (!db.commit()) {
        db.rollback();
        QMessageBox::critical(this, "Ошибка", "Не удалось зафиксировать транзакцию");
        return;
    }

    PostActionLogger::log(m_editMode ? "UPDATE" : "POST", "tblstatuschangedocs", docId);
    PostActionLogger::notify();
    QMessageBox::information(
        this, "Успех", QString("Документ «%1» успешно %2!").arg(actionTitle(), m_editMode ? "обновлён" : "проведён"));
    this->close();
}

void StatusChangeForm::loadForEdit(int docId)
{
    m_editMode = true;
    m_editDocId = docId;
    m_originalTerminals.clear();
    m_originalStatus.clear();
    m_originalActionType.clear();

    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    QSqlQuery query(db);
    query.prepare("SELECT docnumber, docdate, actiontype, comment, basedocid "
                  "FROM tblstatuschangedocs WHERE statuschangedocid = :id");
    query.bindValue(":id", docId);

    if (!query.exec() || !query.next()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось загрузить документ");
        return;
    }

    QString docNumber = query.value(0).toString();
    QDateTime docDate = query.value(1).toDateTime();
    QString actionType = query.value(2).toString();
    QString comment = query.value(3).toString();
    int basedocid = query.value(4).toInt();

    m_originalActionType = actionType;

    ui->lineEditNumber->setText(docNumber);
    ui->lineEditNumber->setReadOnly(true);
    ui->dateEdit->setDate(docDate.date());
    ui->textEditComment->setText(comment);

    int typeIndex = ui->comboBoxActionType->findData(actionType);
    if (typeIndex >= 0) {
        ui->comboBoxActionType->setCurrentIndex(typeIndex);
    }

    if (actionType == "repair_return" && basedocid > 0) {
        int docIndex = ui->comboBoxRepairDoc->findData(basedocid);
        if (docIndex >= 0) {
            ui->comboBoxRepairDoc->setCurrentIndex(docIndex);
        }
    }

    QSqlQuery detailQuery(db);
    detailQuery.prepare("SELECT terminalid, old_status FROM tblstatuschangedetails WHERE statuschangedocid = :id");
    detailQuery.bindValue(":id", docId);

    QList<int> docTerminals;
    if (detailQuery.exec()) {
        while (detailQuery.next()) {
            int tid = detailQuery.value(0).toInt();
            docTerminals.append(tid);
            m_originalTerminals.insert(tid);
            if (!detailQuery.value(1).isNull()) {
                m_originalStatus.insert(tid, detailQuery.value(1).toInt());
            }
        }
    }

    for (int i = 0; i < rowsModel->rowCount(); ++i) {
        QStandardItem* checkItem = rowsModel->item(i, 0);
        if (checkItem && docTerminals.contains(checkItem->data(Qt::UserRole).toInt())) {
            checkItem->setCheckState(Qt::Checked);
        }
    }

    updateWindowTitle();
}

void StatusChangeForm::on_btnPrint_clicked()
{
    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    if (!m_editMode && ui->lineEditNumber->text().trimmed().isEmpty()) {
        QString num = DocumentNumberGenerator::generate("statuschange", db);
        if (num.isEmpty()) {
            QMessageBox::critical(this, "Ошибка БД", "Не удалось сгенерировать номер документа.");
            return;
        }
        ui->lineEditNumber->setText(num);
    }

    QString type = actionType();
    QString title;
    if (type == "repair")
        title = "АКТ ПЕРЕДАЧИ В РЕМОНТ";
    else if (type == "repair_return")
        title = "АКТ ВОЗВРАТА ИЗ РЕМОНТА";
    else if (type == "writeoff")
        title = "АКТ СПИСАНИЯ";
    else if (type == "lost")
        title = "АКТ УТЕРИ";
    else
        return;

    QString html = PrintService::docHeader();

    html += "<h2>" + title.toHtmlEscaped() + " № " + ui->lineEditNumber->text().toHtmlEscaped() + "</h2>";
    html += "<p>от " + ui->dateEdit->date().toString("dd.MM.yyyy") + " г.</p>";
    html += "<p><b>Организация:</b> ООО «POC Terminal»</p>";
    html += "<p><b>Комментарий:</b> " + ui->textEditComment->toPlainText().toHtmlEscaped() + "</p>";

    html += "<table><tr><th>№</th><th>Серийный номер</th><th>Модель</th><th>IMEI 1</th></tr>";

    int num = 0;
    for (int i = 0; i < rowsModel->rowCount(); ++i) {
        QStandardItem* checkItem = rowsModel->item(i, 0);
        if (!checkItem || checkItem->checkState() != Qt::Checked)
            continue;
        ++num;
        html += "<tr><td>" + QString::number(num) +
                "</td>"
                "<td>" +
                rowsModel->item(i, 1)->text().toHtmlEscaped() +
                "</td>"
                "<td>" +
                rowsModel->item(i, 2)->text().toHtmlEscaped() +
                "</td>"
                "<td>" +
                rowsModel->item(i, 3)->text().toHtmlEscaped() + "</td></tr>";
    }
    html += "</table>";

    html += "<div style='margin-top: 40px; display: flex; justify-content: space-between;'>"
            "<div><p>Составил:</p><p>________________ / ____________</p></div>"
            "</div>";
    html += PrintService::docFooter();

    PrintService::printHtml(html, this);
}

void StatusChangeForm::on_btnClose_clicked()
{
    close();
}
