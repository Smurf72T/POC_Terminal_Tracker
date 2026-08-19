#include "rentalform.h"
#include "ui_rentalform.h"
#include "delegates/comboboxdelegate.h"
#include "delegates/comboboxmodel.h"
#include "database/databasemanager.h"
#include "database/repositories/clientrepository.h"
#include "database/repositories/documentrepository.h"
#include "database/repositories/simcardrepository.h"
#include "database/repositories/terminalrepository.h"
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QTime>
#include <QDebug>
#include <QSqlRecord>
#include "utils/logging.h"
#include "services/documentnumbergenerator.h"
#include "services/postactionlogger.h"
#include "services/simcardservice.h"
#include "ui/base/printservice.h"
#include "ui/base/transactionguard.h"
#include <QSet>
#include <QHash>
#include <QSqlDatabase>

bool RentalForm::validateBeforePost()
{
    if (rowsModel->rowCount() == 0) {
        QMessageBox::warning(this, "Внимание", "Добавьте хотя бы одну строку!");
        return false;
    }

    int clientId = ui->comboBoxClient->currentData().toInt();
    if (clientId == 0) {
        QMessageBox::warning(this, "Внимание", "Выберите клиента!");
        return false;
    }

    return true;
}

int RentalForm::postHeader(QSqlDatabase& db)
{
    int clientId = ui->comboBoxClient->currentData().toInt();
    QSqlQuery query(db);

    int docId;

    if (m_editMode) {
        query.prepare(
            "UPDATE tblrentaldocs SET docdate = :date, clientid = :client, comments = :comm WHERE rentaldocid = :id");
        query.bindValue(":id", m_editDocId);
        query.bindValue(":date", QDateTime(ui->dateEdit->date(), QTime::currentTime()));
        query.bindValue(":client", clientId);
        query.bindValue(":comm", ui->textEditComment->toPlainText());

        if (!query.exec()) {
            QMessageBox::critical(this, "Ошибка БД", "Не удалось обновить шапку: " + query.lastError().text());
            return -1;
        }

        docId = m_editDocId;

        QSqlQuery deleteQuery(db);
        deleteQuery.prepare("DELETE FROM tblrentaldetails WHERE rentaldocid = :id");
        deleteQuery.bindValue(":id", docId);
        if (!deleteQuery.exec()) {
            QMessageBox::critical(this, "Ошибка БД",
                                  "Не удалось удалить старые строки: " + deleteQuery.lastError().text());
            return -1;
        }
    } else {
        if (ui->lineEditNumber->text().trimmed().isEmpty()) {
            QString num = DocumentNumberGenerator::generate("rental", db);
            if (num.isEmpty()) {
                QMessageBox::critical(this, "Ошибка БД", "Не удалось сгенерировать номер документа.");
                return -1;
            }
            ui->lineEditNumber->setText(num);
        }
        query.prepare("INSERT INTO tblrentaldocs (docnumber, docdate, clientid, comments) "
                      "VALUES (:num, :date, :client, :comm) RETURNING rentaldocid");
        query.bindValue(":num", ui->lineEditNumber->text());
        query.bindValue(":date", QDateTime(ui->dateEdit->date(), QTime::currentTime()));
        query.bindValue(":client", clientId);
        query.bindValue(":comm", ui->textEditComment->toPlainText());

        if (!query.exec() || !query.next()) {
            QMessageBox::critical(this, "Ошибка БД", "Не удалось создать шапку: " + query.lastError().text());
            return -1;
        }
        docId = query.value(0).toInt();
    }
}

bool RentalForm::postDetails(QSqlDatabase& db, int docId)
{
    // Терминалы, числившиеся в документе до редактирования
    QSet<int> previousTerminals;
    const QList<int> originalKeys = m_originalDetails.keys();
    for (int k : originalKeys)
        previousTerminals.insert(k);

    for (int i = 0; i < rowsModel->rowCount(); ++i) {
        int terminalId = rowsModel->data(rowsModel->index(i, 0), Qt::UserRole).toInt();
        int sim1Id = rowsModel->data(rowsModel->index(i, 1), Qt::UserRole).toInt();
        QString sim1Number = rowsModel->data(rowsModel->index(i, 1), Qt::DisplayRole).toString().trimmed();
        int sim2Id = rowsModel->data(rowsModel->index(i, 2), Qt::UserRole).toInt();
        QString sim2Number = rowsModel->data(rowsModel->index(i, 2), Qt::DisplayRole).toString().trimmed();
        QString comment = rowsModel->data(rowsModel->index(i, 3), Qt::DisplayRole).toString();

        if (terminalId <= 0) {
            QMessageBox::critical(this, "Ошибка", QString("Строка %1: выберите терминал.").arg(i + 1));
            return false;
        }

        bool wasInDoc = previousTerminals.contains(terminalId);
        const QPair<int, int> original =
            m_editMode ? m_originalDetails.value(terminalId, qMakePair(0, 0)) : qMakePair(0, 0);
        int origSim1 = original.first;
        int origSim2 = original.second;

        // Введён новый номер SIM — создаём карточку в справочнике (или берём
        // существующую с таким же номером). Для каждого слота отдельно.
        QString simError;
        sim1Id = SimCardService::resolveOrCreate(db, sim1Id, sim1Number, &simError);
        if (sim1Id < 0) {
            QMessageBox::critical(this, "Ошибка", simError);
            return false;
        }
        sim2Id = SimCardService::resolveOrCreate(db, sim2Id, sim2Number, &simError);
        if (sim2Id < 0) {
            QMessageBox::critical(this, "Ошибка", simError);
            return false;
        }

        // Одна и та же SIM не может стоять в двух слотах одного терминала.
        if (sim1Id > 0 && sim1Id == sim2Id) {
            QMessageBox::critical(this, "Ошибка",
                                  QString("Строка %1: одна и та же SIM-карта указана в слотах 1 и 2.").arg(i + 1));
            return false;
        }

        // Блокируем терминал и проверяем его состояние
        QSqlQuery checkQuery(db);
        checkQuery.prepare("SELECT status FROM tblterminals WHERE terminalid = :id FOR UPDATE NOWAIT");
        checkQuery.bindValue(":id", terminalId);

        if (!checkQuery.exec() || !checkQuery.next()) {
            QMessageBox::critical(
                this, "Ошибка",
                QString("Не удалось заблокировать терминал %1. Возможно, он уже сдан в аренду.").arg(terminalId));
            return false;
        }
        int status = checkQuery.value(0).toInt();

        if (!wasInDoc) {
            // Новый терминал в документе: должен быть свободен
            if (status != 0) {
                QMessageBox::critical(this, "Ошибка", QString("Терминал %1 больше не свободен!").arg(terminalId));
                return false;
            }
        } else if (status != 1) {
            QMessageBox::critical(this, "Ошибка",
                                  QString("Терминал %1 уже не числится в аренде по этому документу.").arg(terminalId));
            return false;
        }

        bool sim1Changed = sim1Id != origSim1;
        bool sim2Changed = sim2Id != origSim2;

        // Освобождаем прежние SIM, если привязка в слоте изменилась
        if (wasInDoc && sim1Changed && origSim1 > 0) {
            if (!SimCardService::free(db, origSim1, QString("слот 1, терминал %1").arg(terminalId), &simError)) {
                QMessageBox::critical(this, "Ошибка БД", simError);
                return false;
            }
        }
        if (wasInDoc && sim2Changed && origSim2 > 0) {
            if (!SimCardService::free(db, origSim2, QString("слот 2, терминал %1").arg(terminalId), &simError)) {
                QMessageBox::critical(this, "Ошибка БД", simError);
                return false;
            }
        }

        // Занимаем новые SIM (новый терминал или замена SIM в существующей строке)
        if (sim1Id > 0 && sim1Changed) {
            if (!SimCardService::lock(db, sim1Id, QString("SIM-карта %1").arg(sim1Number), &simError)) {
                QMessageBox::critical(this, "Ошибка", simError);
                return false;
            }
        }
        if (sim2Id > 0 && sim2Changed) {
            if (!SimCardService::lock(db, sim2Id, QString("SIM-карта %1").arg(sim2Number), &simError)) {
                QMessageBox::critical(this, "Ошибка", simError);
                return false;
            }
        }

        if (!wasInDoc) {
            // Новый терминал — переводим в аренду и привязываем SIM обоих слотов
            QSqlQuery updateQuery(db);
            updateQuery.prepare("UPDATE tblterminals SET status = 1, currentsimcardid = :sim1, "
                                "currentsimcardid2 = :sim2 WHERE terminalid = :id");
            updateQuery.bindValue(":id", terminalId);
            updateQuery.bindValue(":sim1", sim1Id > 0 ? QVariant(sim1Id) : QVariant());
            updateQuery.bindValue(":sim2", sim2Id > 0 ? QVariant(sim2Id) : QVariant());
            if (!updateQuery.exec()) {
                QMessageBox::critical(
                    this, "Ошибка БД",
                    QString("Не удалось обновить терминал %1: %2").arg(terminalId).arg(updateQuery.lastError().text()));
                return false;
            }
        } else if (sim1Changed || sim2Changed) {
            // Существующий терминал — обновляем только привязки SIM
            QSqlQuery updateQuery(db);
            updateQuery.prepare("UPDATE tblterminals SET currentsimcardid = :sim1, "
                                "currentsimcardid2 = :sim2 WHERE terminalid = :id");
            updateQuery.bindValue(":id", terminalId);
            updateQuery.bindValue(":sim1", sim1Id > 0 ? QVariant(sim1Id) : QVariant());
            updateQuery.bindValue(":sim2", sim2Id > 0 ? QVariant(sim2Id) : QVariant());
            if (!updateQuery.exec()) {
                QMessageBox::critical(
                    this, "Ошибка БД",
                    QString("Не удалось обновить терминал %1: %2").arg(terminalId).arg(updateQuery.lastError().text()));
                return false;
            }
        }

        QSqlQuery detailQuery(db);
        detailQuery.prepare("INSERT INTO tblrentaldetails (rentaldocid, terminalid, simcardid, simcardid2, comment) "
                            "VALUES (:did, :tid, :sid, :sid2, :comm)");
        detailQuery.bindValue(":did", docId);
        detailQuery.bindValue(":tid", terminalId);
        detailQuery.bindValue(":sid", sim1Id > 0 ? QVariant(sim1Id) : QVariant());
        detailQuery.bindValue(":sid2", sim2Id > 0 ? QVariant(sim2Id) : QVariant());
        detailQuery.bindValue(":comm", comment);

        if (!detailQuery.exec()) {
            QMessageBox::critical(this, "Ошибка БД", "Ошибка связи: " + detailQuery.lastError().text());
            return false;
        }
    }

    // В режиме редактирования освобождаем терминалы, удалённые из документа
    if (m_editMode) {
        for (int tid : previousTerminals) {
            bool stillInDoc = false;
            for (int i = 0; i < rowsModel->rowCount(); ++i) {
                if (rowsModel->data(rowsModel->index(i, 0), Qt::UserRole).toInt() == tid) {
                    stillInDoc = true;
                    break;
                }
            }
            if (stillInDoc)
                continue;

            QSqlQuery lockQuery(db);
            lockQuery.prepare("SELECT status, currentsimcardid, currentsimcardid2 "
                              "FROM tblterminals WHERE terminalid = :id FOR UPDATE NOWAIT");
            lockQuery.bindValue(":id", tid);
            if (!lockQuery.exec() || !lockQuery.next())
                continue;

            int tStatus = lockQuery.value(0).toInt();
            int tSim1 = lockQuery.value(1).toInt();
            int tSim2 = lockQuery.value(2).toInt();
            if (tStatus != 1)
                continue;

            QString simError;
            if (tSim1 > 0 && !SimCardService::free(db, tSim1, QString("терминал %1").arg(tid), &simError)) {
                QMessageBox::critical(this, "Ошибка БД", simError);
                return false;
            }
            if (tSim2 > 0 && !SimCardService::free(db, tSim2, QString("терминал %1").arg(tid), &simError)) {
                QMessageBox::critical(this, "Ошибка БД", simError);
                return false;
            }

            QSqlQuery upd(db);
            upd.prepare("UPDATE tblterminals SET status = 0, currentsimcardid = NULL, currentsimcardid2 = NULL "
                        "WHERE terminalid = :id");
            upd.bindValue(":id", tid);
            if (!upd.exec()) {
                QMessageBox::critical(
                    this, "Ошибка БД",
                    QString("Не удалось освободить терминал %1: %2").arg(tid).arg(upd.lastError().text()));
                return false;
            }
        }
    }
    return true;
}
