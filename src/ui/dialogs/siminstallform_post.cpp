#include "siminstallform.h"
#include "ui_siminstallform.h"
#include "database/databasemanager.h"
#include "database/repositories/simcardrepository.h"
#include "database/repositories/terminalrepository.h"
#include "database/repositories/siminstallrepository.h"
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QTime>
#include <QDebug>
#include "utils/logging.h"
#include "services/documentnumbergenerator.h"
#include "services/simcardservice.h"
#include "ui/base/transactionguard.h"
#include <QSet>

bool SimInstallForm::validateBeforePost()
{
    if (rowsModel->rowCount() == 0) {
        QMessageBox::warning(this, "Внимание", "Добавьте хотя бы одну строку!");
        return false;
    }
    return true;
}

int SimInstallForm::postHeader(QSqlDatabase& db)
{
    QSqlQuery query(db);

    if (m_editMode) {
        query.prepare("UPDATE tblsiminstalldocs SET docdate = :date, comments = :comm "
                      "WHERE siminstalldocid = :id");
        query.bindValue(":date", QDateTime(ui->dateEdit->date(), QTime::currentTime()));
        query.bindValue(":comm", ui->textEditComment->toPlainText());
        query.bindValue(":id", m_editDocId);

        if (!query.exec()) {
            QMessageBox::critical(this, "Ошибка БД", "Не удалось обновить шапку: " + query.lastError().text());
            return -1;
        }

        query.prepare("DELETE FROM tblsiminstalldetails WHERE siminstalldocid = :id");
        query.bindValue(":id", m_editDocId);
        if (!query.exec()) {
            QMessageBox::critical(this, "Ошибка БД", "Не удалось удалить строки: " + query.lastError().text());
            return -1;
        }

        return m_editDocId;
    }

    if (ui->lineEditNumber->text().trimmed().isEmpty()) {
        QString num = DocumentNumberGenerator::generate("sim_install", db);
        if (num.isEmpty()) {
            QMessageBox::critical(this, "Ошибка БД", "Не удалось сгенерировать номер документа.");
            return -1;
        }
        ui->lineEditNumber->setText(num);
    }

    query.prepare("INSERT INTO tblsiminstalldocs (docnumber, docdate, comments) "
                  "VALUES (:num, :date, :comm) RETURNING siminstalldocid");
    query.bindValue(":num", ui->lineEditNumber->text());
    query.bindValue(":date", QDateTime(ui->dateEdit->date(), QTime::currentTime()));
    query.bindValue(":comm", ui->textEditComment->toPlainText());

    if (!query.exec() || !query.next()) {
        QMessageBox::critical(this, "Ошибка БД", "Не удалось создать шапку: " + query.lastError().text());
        return -1;
    }
    int docId = query.value(0).toInt();
    return docId;
}

bool SimInstallForm::postDetails(QSqlDatabase& db, int docId)
{
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

        if (terminalId <= 0) {
            QMessageBox::critical(this, "Ошибка", QString("Строка %1: выберите терминал.").arg(i + 1));
            return false;
        }

        if (sim1Id <= 0 && sim2Id <= 0) {
            QMessageBox::critical(this, "Ошибка",
                                  QString("Строка %1: выберите хотя бы одну SIM-карту.").arg(i + 1));
            return false;
        }

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

        if (sim1Id > 0 && sim1Id == sim2Id) {
            QMessageBox::critical(this, "Ошибка",
                                  QString("Строка %1: одна и та же SIM-карта указана в слотах 1 и 2.").arg(i + 1));
            return false;
        }

        // Блокируем терминал — он должен быть свободен (status = 0)
        QSqlQuery checkQuery(db);
        checkQuery.prepare("SELECT status, currentsimcardid, currentsimcardid2 FROM tblterminals "
                           "WHERE terminalid = :id FOR UPDATE NOWAIT");
        checkQuery.bindValue(":id", terminalId);

        if (!checkQuery.exec() || !checkQuery.next()) {
            QMessageBox::critical(
                this, "Ошибка",
                QString("Не удалось заблокировать терминал %1. Возможно, он уже обрабатывается.").arg(terminalId));
            return false;
        }
        int status = checkQuery.value(0).toInt();
        int origSim1 = checkQuery.value(1).toInt();
        int origSim2 = checkQuery.value(2).toInt();

        if (status != 0) {
            QMessageBox::critical(this, "Ошибка", QString("Терминал %1 не свободен!").arg(terminalId));
            return false;
        }

        bool sim1Changed = sim1Id != origSim1;
        bool sim2Changed = sim2Id != origSim2;

        // Освобождаем прежние SIM, если привязка в слоте изменилась/пустой слот
        // означает снятие установленной SIM
        if (sim1Changed && origSim1 > 0) {
            if (!SimCardService::free(db, origSim1, QString("слот 1, терминал %1").arg(terminalId), &simError)) {
                QMessageBox::critical(this, "Ошибка БД", simError);
                return false;
            }
        }
        if (sim2Changed && origSim2 > 0) {
            if (!SimCardService::free(db, origSim2, QString("слот 2, терминал %1").arg(terminalId), &simError)) {
                QMessageBox::critical(this, "Ошибка БД", simError);
                return false;
            }
        }

        // Занимаем новые SIM
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

        // Обновляем привязки SIM в терминале (статус терминала НЕ меняется!)
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

        // Записываем строку документа
        QSqlQuery detailQuery(db);
        detailQuery.prepare("INSERT INTO tblsiminstalldetails (siminstalldocid, terminalid, simcardid, simcardid2) "
                            "VALUES (:did, :tid, :sid, :sid2)");
        detailQuery.bindValue(":did", docId);
        detailQuery.bindValue(":tid", terminalId);
        detailQuery.bindValue(":sid", sim1Id > 0 ? QVariant(sim1Id) : QVariant());
        detailQuery.bindValue(":sid2", sim2Id > 0 ? QVariant(sim2Id) : QVariant());

        if (!detailQuery.exec()) {
            QMessageBox::critical(this, "Ошибка БД", "Ошибка записи строки: " + detailQuery.lastError().text());
            return false;
        }
    }

    // В режиме редактирования: освобождаем терминалы, удалённые из документа
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
            lockQuery.prepare("SELECT currentsimcardid, currentsimcardid2 "
                              "FROM tblterminals WHERE terminalid = :id FOR UPDATE NOWAIT");
            lockQuery.bindValue(":id", tid);
            if (!lockQuery.exec() || !lockQuery.next())
                continue;

            int tSim1 = lockQuery.value(0).toInt();
            int tSim2 = lockQuery.value(1).toInt();

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
            upd.prepare("UPDATE tblterminals SET currentsimcardid = NULL, currentsimcardid2 = NULL "
                        "WHERE terminalid = :id");
            upd.bindValue(":id", tid);
            if (!upd.exec()) {
                QMessageBox::critical(
                    this, "Ошибка БД",
                    QString("Не удалось очистить SIM терминала %1: %2").arg(tid).arg(upd.lastError().text()));
                return false;
            }
        }
    }

    return true;
}
