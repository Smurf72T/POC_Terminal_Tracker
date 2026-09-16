#include "caseinstallform.h"
#include "ui_caseinstallform.h"
#include "database/databasemanager.h"
#include "database/repositories/caserepository.h"
#include "services/caseservice.h"
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
enum ColCaseInstall { ColTerminal = 0, ColCase = 1 };
}

bool CaseInstallForm::validateBeforePost()
{
    if (rowsModel->rowCount() == 0) {
        QMessageBox::warning(this, "Внимание", "Добавьте хотя бы одну строку!");
        return false;
    }

    QSet<int> usedTerminals;
    for (int r = 0; r < rowsModel->rowCount(); ++r) {
        int terminalId = rowsModel->data(rowsModel->index(r, ColTerminal), Qt::UserRole).toInt();
        if (terminalId <= 0) {
            QMessageBox::critical(this, "Ошибка", QString("Строка %1: выберите терминал.").arg(r + 1));
            return false;
        }
        if (usedTerminals.contains(terminalId)) {
            QMessageBox::critical(this, "Ошибка",
                                  QString("Строка %1: терминал уже указан в документе.").arg(r + 1));
            return false;
        }
        usedTerminals.insert(terminalId);
    }
    return true;
}

int CaseInstallForm::postHeader(QSqlDatabase& db)
{
    QSqlQuery query(db);
    int docId;

    if (m_editMode) {
        query.prepare("UPDATE tblcaseinstalldocs SET docdate = :date, comments = :comm WHERE caseinstalldocid = :id");
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
        query.prepare("INSERT INTO tblcaseinstalldocs (docnumber, docdate, comments) "
                      "VALUES (:num, :date, :comm) RETURNING caseinstalldocid");
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

bool CaseInstallForm::postDetails(QSqlDatabase& db, int docId)
{
    QSet<int> previousTerminals;
    const QList<int> originalKeys = m_originalDetails.keys();
    for (int k : originalKeys)
        previousTerminals.insert(k);

    for (int i = 0; i < rowsModel->rowCount(); ++i) {
        int terminalId = rowsModel->data(rowsModel->index(i, ColTerminal), Qt::UserRole).toInt();
        int caseId = rowsModel->data(rowsModel->index(i, ColCase), Qt::UserRole).toInt();
        QString caseType = rowsModel->data(rowsModel->index(i, ColCase), Qt::DisplayRole).toString().trimmed();

        if (terminalId <= 0) {
            QMessageBox::critical(this, "Ошибка", QString("Строка %1: выберите терминал.").arg(i + 1));
            return false;
        }

        // Блокируем терминал — он должен быть свободен (0) или в аренде (1).
        QSqlQuery checkQuery(db);
        checkQuery.prepare("SELECT status, COALESCE(currentcaseid, 0) FROM tblterminals "
                           "WHERE terminalid = :id FOR UPDATE NOWAIT");
        checkQuery.bindValue(":id", terminalId);

        if (!checkQuery.exec() || !checkQuery.next()) {
            QMessageBox::critical(
                this, "Ошибка",
                QString("Не удалось заблокировать терминал %1. Возможно, он уже обрабатывается.").arg(terminalId));
            return false;
        }
        int status = checkQuery.value(0).toInt();
        int origCase = checkQuery.value(1).toInt();

        if (status != 0 && status != 1) {
            QMessageBox::critical(this, "Ошибка",
                                  QString("Терминал %1 должен быть свободен или в аренде (текущий статус: %2).")
                                      .arg(terminalId)
                                      .arg(status));
            return false;
        }

        QString caseError;
        if (caseId > 0 && caseId == origCase) {
            // Чехол уже установлен — привязку не трогаем.
        } else {
            if (origCase > 0 && !CaseService::free(db, origCase,
                                                   QString("чехол терминала %1").arg(terminalId), &caseError)) {
                QMessageBox::critical(this, "Ошибка БД", caseError);
                return false;
            }
            if (caseId > 0 && !CaseService::lock(db, caseId, terminalId,
                                                 QString("чехол %1").arg(caseType), &caseError)) {
                QMessageBox::critical(this, "Ошибка", caseError);
                return false;
            }
        }

        QSqlQuery detailQuery(db);
        detailQuery.prepare("INSERT INTO tblcaseinstalldetails (caseinstalldocid, terminalid, caseid) "
                            "VALUES (:did, :tid, :cid)");
        detailQuery.bindValue(":did", docId);
        detailQuery.bindValue(":tid", terminalId);
        detailQuery.bindValue(":cid", caseId > 0 ? QVariant(caseId) : QVariant());

        if (!detailQuery.exec()) {
            QMessageBox::critical(this, "Ошибка БД",
                                  QString("Строка %1: не удалось сохранить: %2").arg(i + 1).arg(detailQuery.lastError().text()));
            return false;
        }
    }

    // В режиме редактирования: освобождаем чехлы терминалов, удалённых из документа.
    if (m_editMode) {
        for (int tid : previousTerminals) {
            bool stillInDoc = false;
            for (int i = 0; i < rowsModel->rowCount(); ++i) {
                if (rowsModel->data(rowsModel->index(i, ColTerminal), Qt::UserRole).toInt() == tid) {
                    stillInDoc = true;
                    break;
                }
            }
            if (stillInDoc)
                continue;

            QSqlQuery lockQuery(db);
            lockQuery.prepare("SELECT COALESCE(currentcaseid, 0) FROM tblterminals "
                              "WHERE terminalid = :id FOR UPDATE NOWAIT");
            lockQuery.bindValue(":id", tid);
            if (!lockQuery.exec() || !lockQuery.next())
                continue;

            int tCase = lockQuery.value(0).toInt();
            QString caseError;
            if (tCase > 0 && !CaseService::free(db, tCase, QString("терминал %1").arg(tid), &caseError)) {
                QMessageBox::critical(this, "Ошибка БД", caseError);
                return false;
            }
        }
    }

    return true;
}