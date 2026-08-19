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
