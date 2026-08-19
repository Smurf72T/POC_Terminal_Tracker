#include "receiptform.h"
#include "database/repositories/terminalrepository.h"
#include "services/serialunitsservice.h"
#include <QDateEdit>
#include <QDateTime>
#include <QLineEdit>
#include <QMessageBox>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextEdit>
#include <QTime>

bool ReceiptForm::validateBeforePost()
{
    if (rowsModel->rowCount() == 0) {
        QMessageBox::warning(this, "Внимание", "Добавьте хотя бы одну строку!");
        return false;
    }

    // 0. Собираем строки и проверяем дубли в рамках документа.
    m_units.clear();
    QSet<QString> usedSerials, usedImei1, usedImei2;

    // Проверка одного IMEI: ровно 15 цифр и отсутствие дублей в документе.
    const auto checkImei = [this](const QString& imei, QSet<QString>& used, const QString& label, int r, int k) -> bool {
        if (imei.isEmpty())
            return true;
        if (!SerialUnitsService::isValidImei(imei)) {
            QMessageBox::critical(this, "Ошибка",
                                  QString("Строка %1, комплект %2: %3 должен содержать ровно 15 цифр (сейчас: %4)")
                                      .arg(r + 1)
                                      .arg(k + 1)
                                      .arg(label)
                                      .arg(imei));
            return false;
        }
        if (used.contains(imei)) {
            QMessageBox::critical(this, "Ошибка", QString("%1 повторяется в документе: %2").arg(label).arg(imei));
            return false;
        }
        used.insert(imei);
        return true;
    };

    for (int r = 0; r < rowsModel->rowCount(); ++r) {
        UnitData it;
        it.modelId = rowsModel->data(rowsModel->index(r, ColModel), Qt::UserRole).toInt();
        it.qty = rowQty(r);
        it.serials = serialsForRow(r);
        QStringList imei1 = SerialUnitsService::alignedImei(it.serials, imei1ForRow(r));
        QStringList imei2 = SerialUnitsService::alignedImei(it.serials, imei2ForRow(r));
        it.imei1 = imei1;
        it.imei2 = imei2;

        if (it.modelId <= 0) {
            QMessageBox::critical(this, "Ошибка", QString("Строка %1: выберите модель из списка.").arg(r + 1));
            return false;
        }
        if (it.serials.size() != it.qty) {
            QMessageBox::critical(this, "Ошибка",
                                  QString("Строка %1: введено %2 серийных номеров из %3.")
                                      .arg(r + 1)
                                      .arg(it.serials.size())
                                      .arg(it.qty));
            return false;
        }
        // Серийник в каждом комплекте обязателен.
        for (int k = 0; k < it.serials.size(); ++k) {
            if (!SerialUnitsService::isValidSerial(it.serials.at(k))) {
                QMessageBox::critical(this, "Ошибка",
                                      QString("Строка %1, комплект %2: не заполнен серийный номер.")
                                          .arg(r + 1)
                                          .arg(k + 1));
                return false;
            }
        }
        if (it.imei1.size() != it.serials.size() || it.imei2.size() != it.serials.size()) {
            QMessageBox::critical(this, "Ошибка",
                                  QString("Строка %1: количество IMEI не соответствует числу серийных номеров.")
                                      .arg(r + 1));
            return false;
        }
        for (int k = 0; k < it.serials.size(); ++k) {
            const QString sn = it.serials.at(k).trimmed();
            const QString im1 = it.imei1.at(k).trimmed();
            const QString im2 = it.imei2.at(k).trimmed();

            if (usedSerials.contains(sn)) {
                QMessageBox::critical(this, "Ошибка",
                                      QString("Серийный номер повторяется в документе: %1").arg(sn));
                return false;
            }
            usedSerials.insert(sn);

            if (!checkImei(im1, usedImei1, "IMEI 1", r, k))
                return false;
            if (!checkImei(im2, usedImei2, "IMEI 2", r, k))
                return false;
        }

        m_units.append(it);
    }
    return true;
}

int ReceiptForm::postHeader(QSqlDatabase& db)
{
    QSqlQuery query(db);
    int docId;

    // 1. Шапка документа.
    if (m_editMode) {
        query.prepare("UPDATE tblreceiptdocs SET docdate = :date, comments = :comm WHERE receiptdocid = :id");
        query.bindValue(":date", QDateTime(headerDateEdit()->date(), QTime::currentTime()));
        query.bindValue(":comm", headerCommentEdit()->toPlainText());
        query.bindValue(":id", m_editDocId);

        if (!query.exec()) {
            QMessageBox::critical(this, "Ошибка БД", "Не удалось обновить шапку: " + query.lastError().text());
            return -1;
        }
        docId = m_editDocId;
    } else {
        if (!ensureDocNumber())
            return -1;
        query.prepare("INSERT INTO tblreceiptdocs (docnumber, docdate, comments) "
                      "VALUES (:num, :date, :comm) RETURNING receiptdocid");
        query.bindValue(":num", headerNumberEdit()->text());
        query.bindValue(":date", QDateTime(headerDateEdit()->date(), QTime::currentTime()));
        query.bindValue(":comm", headerCommentEdit()->toPlainText());

        if (!query.exec() || !query.next()) {
            QMessageBox::critical(this, "Ошибка БД", "Не удалось создать шапку: " + query.lastError().text());
            return -1;
        }
        docId = query.value(0).toInt();
    }
    return docId;
}

bool ReceiptForm::postDetails(QSqlDatabase& db, int docId)
{
    // 2. В режиме редактирования пересоздаём «исходник» и связи.
    if (m_editMode) {
        QSqlQuery delItems(db);
        delItems.prepare("DELETE FROM tblreceiptitems WHERE receiptdocid = :id");
        delItems.bindValue(":id", docId);
        if (!delItems.exec()) {
            QMessageBox::critical(this, "Ошибка БД",
                                  "Не удалось удалить старые строки: " + delItems.lastError().text());
            return false;
        }

        QSqlQuery delDetails(db);
        delDetails.prepare("DELETE FROM tblreceiptdetails WHERE receiptdocid = :id");
        delDetails.bindValue(":id", docId);
        if (!delDetails.exec()) {
            QMessageBox::critical(this, "Ошибка БД",
                                  "Не удалось удалить старые связи: " + delDetails.lastError().text());
            return false;
        }
    }

    // 3. Разворачиваем строки в терминалы и сохраняем «исходник».
    for (const UnitData& it : m_units) {
        QSqlQuery itemQuery(db);
        itemQuery.prepare("INSERT INTO tblreceiptitems (receiptdocid, modelid, qty) "
                          "VALUES (:did, :mid, :qty) RETURNING receiptitemid");
        itemQuery.bindValue(":did", docId);
        itemQuery.bindValue(":mid", it.modelId);
        itemQuery.bindValue(":qty", it.qty);
        if (!itemQuery.exec() || !itemQuery.next()) {
            QMessageBox::critical(this, "Ошибка БД",
                                  "Не удалось сохранить строку документа: " + itemQuery.lastError().text());
            return false;
        }
        const int itemId = itemQuery.value(0).toInt();

        for (int i = 0; i < it.serials.size(); ++i) {
            const QString serial = it.serials.at(i).trimmed();
            const QString im1 = it.imei1.at(i).trimmed();
            const QString im2 = it.imei2.at(i).trimmed();

            QSqlQuery serialQuery(db);
            serialQuery.prepare("INSERT INTO tblreceiptserials (receiptitemid, linenum, serialnumber, imei1, imei2) "
                                "VALUES (:iid, :ln, :sn, :i1, :i2)");
            serialQuery.bindValue(":iid", itemId);
            serialQuery.bindValue(":ln", i + 1);
            serialQuery.bindValue(":sn", serial);
            serialQuery.bindValue(":i1", im1);
            serialQuery.bindValue(":i2", im2);
            if (!serialQuery.exec()) {
                QMessageBox::critical(this, "Ошибка БД",
                                      QString("Ошибка сохранения серийного номера %1:\n%2")
                                          .arg(serial, serialQuery.lastError().text()));
                return false;
            }

            int newTermId;
            const int foundId = TerminalRepository(db).findIdBySerial(serial);
            const bool found = foundId > 0;

            if (found && m_editMode) {
                // Редактирование: серийник уже был в базе — обновляем терминал
                // вместе с его IMEI (привязка сохраняется).
                newTermId = foundId;
                QSqlQuery termQuery(db);
                termQuery.prepare("UPDATE tblterminals SET modelid = :mid, imei1 = :i1, imei2 = :i2 "
                                  "WHERE terminalid = :tid");
                termQuery.bindValue(":mid", it.modelId);
                termQuery.bindValue(":i1", im1);
                termQuery.bindValue(":i2", im2);
                termQuery.bindValue(":tid", newTermId);
                if (!termQuery.exec()) {
                    QMessageBox::critical(this, "Ошибка БД",
                                          QString("Ошибка при обновлении терминала %1:\n%2")
                                              .arg(serial, termQuery.lastError().text()));
                    return false;
                }
            } else if (found) {
                QMessageBox::critical(this, "Ошибка",
                                      QString("Серийный номер уже есть в базе: %1").arg(serial));
                return false;
            } else {
                QSqlQuery termQuery(db);
                termQuery.prepare("INSERT INTO tblterminals (serialnumber, modelid, imei1, imei2, status) "
                                  "VALUES (:sn, :mid, :i1, :i2, 0) RETURNING terminalid");
                termQuery.bindValue(":sn", serial);
                termQuery.bindValue(":mid", it.modelId);
                termQuery.bindValue(":i1", im1);
                termQuery.bindValue(":i2", im2);
                if (!termQuery.exec() || !termQuery.next()) {
                    QMessageBox::critical(this, "Ошибка БД",
                                          QString("Ошибка при добавлении терминала %1:\n%2")
                                              .arg(serial, termQuery.lastError().text()));
                    return false;
                }
                newTermId = termQuery.value(0).toInt();
            }

            QSqlQuery detailQuery(db);
            detailQuery.prepare("INSERT INTO tblreceiptdetails (receiptdocid, terminalid) VALUES (:did, :tid)");
            detailQuery.bindValue(":did", docId);
            detailQuery.bindValue(":tid", newTermId);
            if (!detailQuery.exec()) {
                QMessageBox::critical(this, "Ошибка БД", "Ошибка связи: " + detailQuery.lastError().text());
                return false;
            }
        }
    }
    return true;
}
