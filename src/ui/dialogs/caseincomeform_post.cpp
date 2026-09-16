#include "caseincomeform.h"
#include "ui_caseincomeform.h"
#include "database/repositories/caserepository.h"
#include "services/documentnumbergenerator.h"
#include "ui/base/transactionguard.h"
#include <QDateEdit>
#include <QDateTime>
#include <QLineEdit>
#include <QMessageBox>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextEdit>
#include <QTime>

namespace {
enum ColCaseIncome { ColCaseType = 0, ColQty = 1 };
}

bool CaseIncomeForm::validateBeforePost()
{
    if (rowsModel->rowCount() == 0) {
        QMessageBox::warning(this, "Внимание", "Добавьте хотя бы одну строку!");
        return false;
    }

    for (int r = 0; r < rowsModel->rowCount(); ++r) {
        QString type = rowsModel->data(rowsModel->index(r, ColCaseType), Qt::DisplayRole).toString().trimmed();
        int qty = rowsModel->data(rowsModel->index(r, ColQty)).toInt();

        if (type.isEmpty()) {
            QMessageBox::critical(this, "Ошибка", QString("Строка %1: укажите тип чехла.").arg(r + 1));
            return false;
        }
        if (qty <= 0) {
            QMessageBox::critical(this, "Ошибка", QString("Строка %1: количество должно быть больше нуля.").arg(r + 1));
            return false;
        }
    }
    return true;
}

int CaseIncomeForm::postHeader(QSqlDatabase& db)
{
    QSqlQuery query(db);
    int docId;

    if (m_editMode) {
        query.prepare("UPDATE tblcaseincomedocs SET docdate = :date, comments = :comm WHERE caseincomedocid = :id");
        query.bindValue(":date", QDateTime(ui->dateEdit->date(), QTime::currentTime()));
        query.bindValue(":comm", ui->textEditComment->toPlainText());
        query.bindValue(":id", m_editDocId);

        if (!query.exec()) {
            QMessageBox::critical(this, "Ошибка БД", "Не удалось обновить шапку: " + query.lastError().text());
            return -1;
        }
        docId = m_editDocId;
    } else {
        if (!ensureDocNumber())
            return -1;
        query.prepare("INSERT INTO tblcaseincomedocs (docnumber, docdate, comments) "
                      "VALUES (:num, :date, :comm) RETURNING caseincomedocid");
        query.bindValue(":num", ui->lineEditNumber->text());
        query.bindValue(":date", QDateTime(ui->dateEdit->date(), QTime::currentTime()));
        query.bindValue(":comm", ui->textEditComment->toPlainText());

        if (!query.exec() || !query.next()) {
            QMessageBox::critical(this, "Ошибка БД", "Не удалось создать шапку: " + query.lastError().text());
            return -1;
        }
        docId = query.value(0).toInt();
    }
    return docId;
}

bool CaseIncomeForm::postDetails(QSqlDatabase& db, int docId)
{
    CaseRepository repo(db);

    // Новые количества по типам.
    QMap<QString, int> newQty;
    for (int r = 0; r < rowsModel->rowCount(); ++r) {
        QString type = rowsModel->data(rowsModel->index(r, ColCaseType), Qt::DisplayRole).toString().trimmed();
        int qty = rowsModel->data(rowsModel->index(r, ColQty)).toInt();
        newQty[type] += qty;
    }

    // Сверяем с исходником в режиме редактирования.
    if (m_editMode) {
        QSet<QString> handled;
        for (auto it = m_originalQuantities.begin(); it != m_originalQuantities.end(); ++it) {
            handled.insert(it.key());
            int diff = newQty.value(it.key(), 0) - it.value();
            if (diff > 0) {
                if (!repo.createBatch(it.key(), diff)) {
                    QMessageBox::critical(this, "Ошибка БД",
                                          QString("Не удалось добавить чехлы типа «%1»: %2")
                                              .arg(it.key(), db.lastError().text()));
                    return false;
                }
            } else if (diff < 0) {
                // Убираем лишние свободные чехлы. Списанные/установленные не трогаем.
                QSqlQuery pick(db);
                pick.prepare("SELECT caseid FROM tblcases WHERE casetype = :t AND status = 0 "
                             "ORDER BY caseid LIMIT :n FOR UPDATE");
                pick.bindValue(":t", it.key());
                pick.bindValue(":n", -diff);
                if (!pick.exec()) {
                    QMessageBox::critical(this, "Ошибка БД", "Не удалось выбрать чехлы для удаления: " +
                                                                  pick.lastError().text());
                    return false;
                }
                QList<int> toRemove;
                while (pick.next())
                    toRemove.append(pick.value(0).toInt());
                if (toRemove.size() < -diff) {
                    QMessageBox::critical(
                        this, "Ошибка",
                        QString("Недостаточно свободных чехлов типа «%1»: невозможно уменьшить количество "
                                "ниже уже установленных/списанных.")
                            .arg(it.key()));
                    return false;
                }
                for (int caseId : toRemove) {
                    QSqlQuery del(db);
                    del.prepare("DELETE FROM tblcases WHERE caseid = :id");
                    del.bindValue(":id", caseId);
                    if (!del.exec()) {
                        QMessageBox::critical(this, "Ошибка БД",
                                              QString("Не удалось удалить чехол №%1: %2")
                                                  .arg(caseId)
                                                  .arg(del.lastError().text()));
                        return false;
                    }
                }
            }
        }
        // Новые типы, которых не было в исходнике.
        for (auto it = newQty.begin(); it != newQty.end(); ++it) {
            if (handled.contains(it.key()))
                continue;
            if (!repo.createBatch(it.key(), it.value())) {
                QMessageBox::critical(this, "Ошибка БД",
                                      QString("Не удалось добавить чехлы типа «%1»: %2")
                                          .arg(it.key(), db.lastError().text()));
                return false;
            }
        }
    } else {
        // Первичное проведение: создаём свободные чехлы.
        for (auto it = newQty.begin(); it != newQty.end(); ++it) {
            if (!repo.createBatch(it.key(), it.value())) {
                QMessageBox::critical(this, "Ошибка БД",
                                      QString("Не удалось добавить чехлы типа «%1»: %2")
                                          .arg(it.key(), db.lastError().text()));
                return false;
            }
        }
    }

    // Пересоздаём строки документа.
    if (m_editMode) {
        QSqlQuery del(db);
        del.prepare("DELETE FROM tblcaseincomedetails WHERE caseincomedocid = :id");
        del.bindValue(":id", docId);
        if (!del.exec()) {
            QMessageBox::critical(this, "Ошибка БД", "Не удалось удалить старые строки: " + del.lastError().text());
            return false;
        }
    }

    for (int r = 0; r < rowsModel->rowCount(); ++r) {
        QString type = rowsModel->data(rowsModel->index(r, ColCaseType), Qt::DisplayRole).toString().trimmed();
        int qty = rowsModel->data(rowsModel->index(r, ColQty)).toInt();

        QSqlQuery detail(db);
        detail.prepare("INSERT INTO tblcaseincomedetails (caseincomedocid, casetype, qty) "
                       "VALUES (:did, :t, :q)");
        detail.bindValue(":did", docId);
        detail.bindValue(":t", type);
        detail.bindValue(":q", qty);
        if (!detail.exec()) {
            QMessageBox::critical(this, "Ошибка БД",
                                  QString("Строка %1: не удалось сохранить: %2").arg(r + 1).arg(detail.lastError().text()));
            return false;
        }
    }
    return true;
}