#include "statuschangeform.h"
#include "ui_statuschangeform.h"
#include "database/databasemanager.h"
#include "utils/logging.h"
#include "utils/terminal_status.h"
#include "services/documentnumbergenerator.h"
#include "services/postactionlogger.h"
#include "services/statuschangeservice.h"
#include "ui/base/printservice.h"
#include "ui/base/transactionguard.h"
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QTime>
#include <QDebug>

bool StatusChangeForm::validateBeforePost()
{
    m_comment = ui->textEditComment->toPlainText().trimmed();
    if (m_comment.isEmpty()) {
        QMessageBox::warning(this, "Внимание", "Укажите комментарий: что ремонтируется или причина списания/утери.");
        return false;
    }

    m_terminalIds = checkedTerminalIds();
    if (m_terminalIds.isEmpty()) {
        QMessageBox::warning(this, "Внимание", "Отметьте хотя бы один терминал!");
        return false;
    }
    return true;
}

int StatusChangeForm::postHeader(QSqlDatabase& db)
{
    int basedocid = (actionType() == "repair_return") ? ui->comboBoxRepairDoc->currentData().toInt() : 0;

    QSqlQuery query(db);
    int docId = m_editDocId;

    if (m_editMode) {
        query.prepare("UPDATE tblstatuschangedocs SET docdate = :date, actiontype = :type, "
                      "m_comment = :comm, basedocid = :base WHERE statuschangedocid = :id");
        query.bindValue(":date", QDateTime(ui->dateEdit->date(), QTime::currentTime()));
        query.bindValue(":type", actionType());
        query.bindValue(":comm", m_comment);
        if (basedocid > 0) {
            query.bindValue(":base", basedocid);
        } else {
            query.bindValue(":base", QVariant());
        }
        query.bindValue(":id", m_editDocId);
        if (!query.exec()) {
            QMessageBox::critical(this, "Ошибка БД", "Не удалось обновить шапку: " + query.lastError().text());
            return -1;
        }

        query.prepare("DELETE FROM tblstatuschangedetails WHERE statuschangedocid = :id");
        query.bindValue(":id", m_editDocId);
        if (!query.exec()) {
            QMessageBox::critical(this, "Ошибка БД", "Не удалось обновить строки: " + query.lastError().text());
            return -1;
        }
    } else {
        if (ui->lineEditNumber->text().trimmed().isEmpty()) {
            QString num = DocumentNumberGenerator::generate("statuschange", db);
            if (num.isEmpty()) {
                QMessageBox::critical(this, "Ошибка БД", "Не удалось сгенерировать номер документа.");
                return -1;
            }
            ui->lineEditNumber->setText(num);
        }
        query.prepare("INSERT INTO tblstatuschangedocs (docnumber, docdate, actiontype, m_comment, basedocid) "
                      "VALUES (:num, :date, :type, :comm, :base) RETURNING statuschangedocid");
        query.bindValue(":num", ui->lineEditNumber->text());
        query.bindValue(":date", QDateTime(ui->dateEdit->date(), QTime::currentTime()));
        query.bindValue(":type", actionType());
        query.bindValue(":comm", m_comment);
        if (basedocid > 0) {
            query.bindValue(":base", basedocid);
        } else {
            query.bindValue(":base", QVariant());
        }

        if (!query.exec() || !query.next()) {
            QMessageBox::critical(this, "Ошибка БД", "Не удалось создать документ: " + query.lastError().text());
            return -1;
        }
        docId = query.value(0).toInt();
    }
    return docId;
}

bool StatusChangeForm::postDetails(QSqlDatabase& db, int docId)
{
    int target = StatusChangeService::targetStatus(actionType());

    // Терминалы, убранные из документа при редактировании, должны быть
    // возвращены в прежний статус (откат операции).
    QSet<int> checked;
    for (int tid : m_terminalIds)
        checked.insert(tid);
    QList<int> removed;
    if (m_editMode) {
        for (int tid : m_originalTerminals) {
            if (!checked.contains(tid))
                removed.append(tid);
        }
    }

    for (int termId : m_terminalIds) {
        QSqlQuery checkQuery(db);
        checkQuery.prepare("SELECT status FROM tblterminals "
                           "WHERE terminalid = :id FOR UPDATE NOWAIT");
        checkQuery.bindValue(":id", termId);

        if (!checkQuery.exec() || !checkQuery.next()) {
            QMessageBox::critical(
                this, "Ошибка",
                QString("Не удалось заблокировать терминал %1. Возможно, он уже обрабатывается.").arg(termId));
            return false;
        }

        int currentStatus = checkQuery.value(0).toInt();
        bool ok = StatusChangeService::expectStatus(actionType(), currentStatus);
        // При редактировании терминал может уже стоять в целевом статусе —
        // это допустимо (повторное применение идемпотентно).
        if (m_editMode)
            ok = ok || (currentStatus == target);
        if (!ok) {
            QMessageBox::critical(this, "Ошибка",
                                  QString("Терминал %1 имеет статус «%2», операция «%3» для него недоступна.")
                                      .arg(termId)
                                      .arg(StatusChangeService::statusText(currentStatus))
                                      .arg(StatusChangeService::actionTitle(actionType())));
            return false;
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
            QMessageBox::critical(this, "Ошибка БД",
                                  QString("Не удалось обновить статус терминала %1:\n%2")
                                      .arg(termId)
                                      .arg(updateQuery.lastError().text()));
            return false;
        }

        QSqlQuery detailQuery(db);
        detailQuery.prepare("INSERT INTO tblstatuschangedetails (statuschangedocid, terminalid, old_status) "
                            "VALUES (:did, :tid, :old)");
        detailQuery.bindValue(":did", docId);
        detailQuery.bindValue(":tid", termId);
        detailQuery.bindValue(":old", oldStatus);
        if (!detailQuery.exec()) {
            QMessageBox::critical(this, "Ошибка БД", "Ошибка связи: " + detailQuery.lastError().text());
            return false;
        }
    }

    // Откат статусов для терминалов, убранных из документа
    for (int termId : removed) {
        QSqlQuery checkQuery(db);
        checkQuery.prepare("SELECT status FROM tblterminals "
                           "WHERE terminalid = :id FOR UPDATE NOWAIT");
        checkQuery.bindValue(":id", termId);

        if (!checkQuery.exec() || !checkQuery.next()) {
            QMessageBox::critical(
                this, "Ошибка",
                QString("Не удалось заблокировать терминал %1. Возможно, он уже обрабатывается.").arg(termId));
            return false;
        }

        int currentStatus = checkQuery.value(0).toInt();
        if (currentStatus != target) {
            QMessageBox::critical(this, "Ошибка",
                                  QString("Статус терминала %1 («%2») уже изменён другим документом, "
                                          "откат невозможен. Уберите его вручную.")
                                      .arg(termId)
                                      .arg(StatusChangeService::statusText(currentStatus)));
            return false;
        }

        int oldStatus = m_originalStatus.value(termId, -1);
        if (oldStatus < 0) {
            if (m_originalActionType == "repair")
                oldStatus = 0;
            else if (m_originalActionType == "repair_return")
                oldStatus = 2;
            else {
                QMessageBox::critical(this, "Ошибка",
                                      QString("Прежний статус терминала %1 неизвестен (документ проведён до "
                                              "введения снимков статусов). Верните его вручную.")
                                          .arg(termId));
                return false;
            }
        }

        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE tblterminals SET status = :status WHERE terminalid = :id");
        updateQuery.bindValue(":status", oldStatus);
        updateQuery.bindValue(":id", termId);
        if (!updateQuery.exec()) {
            QMessageBox::critical(this, "Ошибка БД",
                                  QString("Не удалось восстановить статус терминала %1:\n%2")
                                      .arg(termId)
                                      .arg(updateQuery.lastError().text()));
            return false;
        }
    }
    return true;
}
