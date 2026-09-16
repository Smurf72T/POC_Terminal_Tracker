#include "casewriteoffform.h"
#include "ui_casewriteoffform.h"
#include "database/databasemanager.h"
#include "services/caseservice.h"
#include "services/documentnumbergenerator.h"
#include "ui/base/transactionguard.h"
#include <QDateEdit>
#include <QDateTime>
#include <QLineEdit>
#include <QMessageBox>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextEdit>
#include <QTime>

namespace {
enum ColCaseWriteoff { ColCheck = 0, ColType = 1, ColTerminal = 2, ColReason = 3 };
}

bool CaseWriteoffForm::validateBeforePost()
{
    bool hasChecked = false;
    for (int r = 0; r < rowsModel->rowCount(); ++r) {
        if (rowsModel->data(rowsModel->index(r, ColCheck), Qt::DisplayRole).toBool()) {
            hasChecked = true;
            break;
        }
    }
    if (!hasChecked) {
        QMessageBox::warning(this, "Внимание", "Отметьте хотя бы один чехол для списания.");
        return false;
    }

    // Отмечаем общие причины для всех отмеченных строк: если заполнено слово
    // «все»/пусто — оставляем как есть, иначе проверяем наличие причины у каждой.
    for (int r = 0; r < rowsModel->rowCount(); ++r) {
        if (!rowsModel->data(rowsModel->index(r, ColCheck), Qt::DisplayRole).toBool())
            continue;
        if (rowsModel->data(rowsModel->index(r, ColReason)).toString().trimmed().isEmpty()) {
            QMessageBox::critical(this, "Ошибка",
                                  QString("Строка %1: укажите причину списания.").arg(r + 1));
            return false;
        }
    }
    return true;
}

int CaseWriteoffForm::postHeader(QSqlDatabase& db)
{
    QSqlQuery query(db);
    int docId;

    if (m_editMode) {
        query.prepare("UPDATE tblcasewriteoffdocs SET docdate = :date, comments = :comm WHERE casewriteoffdocid = :id");
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
        query.prepare("INSERT INTO tblcasewriteoffdocs (docnumber, docdate, comments) "
                      "VALUES (:num, :date, :comm) RETURNING casewriteoffdocid");
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

bool CaseWriteoffForm::postDetails(QSqlDatabase& db, int docId)
{
    struct Row {
        int caseId = 0;
        QString reason;
    };
    QVector<Row> toWriteoff;
    for (int r = 0; r < rowsModel->rowCount(); ++r) {
        if (!rowsModel->data(rowsModel->index(r, ColCheck), Qt::DisplayRole).toBool())
            continue;
        Row row;
        row.caseId = rowsModel->data(rowsModel->index(r, ColCheck), Qt::UserRole).toInt();
        row.reason = rowsModel->data(rowsModel->index(r, ColReason)).toString().trimmed();
        if (row.caseId > 0)
            toWriteoff.append(row);
    }

    // В режиме редактирования восстанавливаем чехлы, снятые с списания.
    if (m_editMode) {
        for (int caseId : m_originalCaseIds) {
            bool stillChecked = false;
            for (const Row& row : toWriteoff) {
                if (row.caseId == caseId) {
                    stillChecked = true;
                    break;
                }
            }
            if (!stillChecked) {
                QSqlQuery restore(db);
                restore.prepare("UPDATE tblcases SET status = 0, terminalid = NULL WHERE caseid = :id");
                restore.bindValue(":id", caseId);
                if (!restore.exec()) {
                    QMessageBox::critical(this, "Ошибка БД",
                                          QString("Не удалось восстановить чехол №%1: %2")
                                              .arg(caseId)
                                              .arg(restore.lastError().text()));
                    return false;
                }
            }
        }
    }

    // Пересоздаём строки документа.
    if (m_editMode) {
        QSqlQuery del(db);
        del.prepare("DELETE FROM tblcasewriteoffdetails WHERE casewriteoffdocid = :id");
        del.bindValue(":id", docId);
        if (!del.exec()) {
            QMessageBox::critical(this, "Ошибка БД", "Не удалось удалить старые строки: " + del.lastError().text());
            return false;
        }
    }

    for (const Row& row : toWriteoff) {
        // Уже списанные (из исходного документа) повторно не списываем.
        if (!(m_editMode && m_originalCaseIds.contains(row.caseId))) {
            QString error;
            if (!CaseService::writeoff(db, row.caseId, QString("чехол №%1").arg(row.caseId), &error)) {
                QMessageBox::critical(this, "Ошибка", error);
                return false;
            }
        }

        QSqlQuery detail(db);
        detail.prepare("INSERT INTO tblcasewriteoffdetails (casewriteoffdocid, caseid, reason) "
                       "VALUES (:did, :cid, :r)");
        detail.bindValue(":did", docId);
        detail.bindValue(":cid", row.caseId);
        detail.bindValue(":r", row.reason);
        if (!detail.exec()) {
            QMessageBox::critical(this, "Ошибка БД",
                                  QString("Не удалось сохранить строку списания чехла №%1: %2")
                                      .arg(row.caseId)
                                      .arg(detail.lastError().text()));
            return false;
        }
    }
    return true;
}