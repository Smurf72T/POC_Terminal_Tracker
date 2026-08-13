#include "rentalform.h"
#include "ui_rentalform.h"
#include "delegates/comboboxdelegate.h"
#include "delegates/comboboxmodel.h"
#include "database/databasemanager.h"
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QTime>
#include <QDebug>
#include <QSqlRecord>
#include "utils/logging.h"
#include <QSet>
#include "utils/reportexporter.h"
#include <QTextDocument>
#include <QPrinter>
#include <QPrintDialog>

RentalForm::RentalForm(QWidget* parent) : QDialog(parent), ui(new Ui::RentalForm)
{
    ui->setupUi(this);
    setWindowTitle("Документ: Передача в аренду");
    resize(900, 600);

    // Настройка даты (сегодня)
    ui->dateEdit->setDate(QDate::currentDate());

    // Номер документа генерируется при проведении (не здесь), чтобы не
    // сжигать значения последовательности для отменённых форм.

    // Настройка модели для табличной части
    rowsModel = new QStandardItemModel(0, 3, this); // 3 колонки
    rowsModel->setHorizontalHeaderLabels({"Терминал", "SIM-карта", "Примечание"});
    ui->tableView->setModel(rowsModel);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);

    // Загружаем данные для выпадающих списков
    loadClientsToDelegate();
    loadFreeTerminalsToDelegate();
    loadFreeSIMsToDelegate();

    // Подключаем сигнал изменения данных
    connect(rowsModel, &QStandardItemModel::dataChanged, this, &RentalForm::onTableViewDataChanged);
}

RentalForm::~RentalForm()
{
    delete ui;
}

void RentalForm::loadClientsToDelegate()
{
    QList<QPair<int, QString>> clients;
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    if (!query.exec("SELECT clientid, clientname FROM tblclients ORDER BY clientname")) {
        qCWarning(logSQL) << "Failed to load clients:" << query.lastError().text();
        return;
    }

    while (query.next()) {
        clients.append(qMakePair(query.value(0).toInt(), query.value(1).toString()));
    }

    // Устанавливаем делегат для колонки клиента
    ui->comboBoxClient->setItemDelegate(new ComboBoxDelegate(clients, this));
    ui->comboBoxClient->setModel(new ComboBoxModel(clients, this));
}

void RentalForm::loadFreeTerminalsToDelegate()
{
    // Загрузим только свободные терминалы
    QList<QPair<int, QString>> terminals;
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    if (!query.exec("SELECT terminalid, serialnumber FROM tblterminals WHERE status = 0 AND is_deactivated = FALSE "
                    "ORDER BY serialnumber")) {
        qCWarning(logSQL) << "Failed to load free terminals:" << query.lastError().text();
        return;
    }

    while (query.next()) {
        terminals.append(qMakePair(query.value(0).toInt(), query.value(1).toString()));
    }

    // Устанавливаем делегат на колонку терминала
    ui->tableView->setItemDelegateForColumn(0, new ComboBoxDelegate(terminals, this));
}

void RentalForm::loadFreeSIMsToDelegate()
{
    QList<QPair<int, QString>> sims;
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    // ИСПРАВЛЕНО: Загружаем SIM-карты, которые:
    // 1. status = 0 (свободны), ИЛИ
    // 2. status = 1 (в аренде), но привязаны к терминалу со статусом 0 (возвращены)
    if (!query.exec("SELECT s.simcardid, s.simnumber "
                    "FROM tblsimcards s "
                    "WHERE s.status = 0 "
                    "OR EXISTS ("
                    "    SELECT 1 FROM tblterminals t "
                    "    WHERE t.currentsimcardid = s.simcardid "
                    "    AND t.status = 0"
                    ")"
                    "ORDER BY s.simnumber")) {
        qCWarning(logSQL) << "Failed to load free SIMs:" << query.lastError().text();
        return;
    }

    while (query.next()) {
        sims.append(qMakePair(query.value(0).toInt(), query.value(1).toString()));
    }

    // Устанавливаем делегат на колонку SIM (редактируемый: можно выбрать
    // существующую SIM-карту или ввести новый номер)
    ui->tableView->setItemDelegateForColumn(1, new ComboBoxDelegate(sims, this, true));
}

void RentalForm::loadForEdit(int docId)
{
    m_editMode = true;
    m_editDocId = docId;
    m_originalDetails.clear();

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("SELECT docnumber, docdate, clientid, comments FROM tblrentaldocs WHERE rentaldocid = :id");
    query.bindValue(":id", docId);

    if (!query.exec() || !query.next()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось загрузить документ: " + query.lastError().text());
        return;
    }

    QString docNumber = query.value(0).toString();
    QDateTime docDate = query.value(1).toDateTime();
    int clientId = query.value(2).toInt();
    QString comments = query.value(3).toString();

    ui->lineEditNumber->setText(docNumber);
    ui->lineEditNumber->setReadOnly(true);
    ui->dateEdit->setDate(docDate.date());
    ui->textEditComment->setText(comments);

    for (int i = 0; i < ui->comboBoxClient->count(); ++i) {
        if (ui->comboBoxClient->itemData(i).toInt() == clientId) {
            ui->comboBoxClient->setCurrentIndex(i);
            break;
        }
    }

    QSqlQuery detailQuery(DatabaseManager::instance().getDatabase());
    detailQuery.prepare("SELECT terminalid, simcardid, comment FROM tblrentaldetails WHERE rentaldocid = :id");
    detailQuery.bindValue(":id", docId);

    if (detailQuery.exec()) {
        while (detailQuery.next()) {
            int termId = detailQuery.value(0).toInt();
            int simId = detailQuery.value(1).toInt();
            QString comment = detailQuery.value(2).toString();

            m_originalDetails.insert(termId, simId);

            int row = rowsModel->rowCount();
            rowsModel->insertRow(row);

            QStandardItem* terminalItem = new QStandardItem();
            terminalItem->setData(termId, Qt::UserRole);

            QSqlQuery termQuery(DatabaseManager::instance().getDatabase());
            termQuery.prepare("SELECT serialnumber FROM tblterminals WHERE terminalid = :id");
            termQuery.bindValue(":id", termId);
            if (termQuery.exec() && termQuery.next()) {
                terminalItem->setData(termQuery.value(0).toString(), Qt::DisplayRole);
            } else {
                terminalItem->setData("", Qt::DisplayRole);
            }

            QStandardItem* simItem = new QStandardItem();
            simItem->setData(simId, Qt::UserRole);

            if (simId > 0) {
                QSqlQuery simQuery(DatabaseManager::instance().getDatabase());
                simQuery.prepare("SELECT simnumber FROM tblsimcards WHERE simcardid = :id");
                simQuery.bindValue(":id", simId);
                if (simQuery.exec() && simQuery.next()) {
                    simItem->setData(simQuery.value(0).toString(), Qt::DisplayRole);
                } else {
                    simItem->setData("", Qt::DisplayRole);
                }
            } else {
                simItem->setData("", Qt::DisplayRole);
            }

            QStandardItem* commentItem = new QStandardItem(comment);

            rowsModel->setItem(row, 0, terminalItem);
            rowsModel->setItem(row, 1, simItem);
            rowsModel->setItem(row, 2, commentItem);
        }
    }

    setWindowTitle(QString("Редактирование аренды ID %1").arg(docId));
}

void RentalForm::on_btnAddRow_clicked()
{
    int row = rowsModel->rowCount();
    rowsModel->insertRow(row);

    // Создаем элементы с пустым текстом и ID = 0
    QStandardItem* terminalItem = new QStandardItem();
    terminalItem->setData(0, Qt::UserRole);     // ID терминала
    terminalItem->setData("", Qt::DisplayRole); // Текст для отображения

    QStandardItem* simItem = new QStandardItem();
    simItem->setData(0, Qt::UserRole);     // ID SIM
    simItem->setData("", Qt::DisplayRole); // Текст для отображения

    QStandardItem* commentItem = new QStandardItem("");

    rowsModel->setItem(row, 0, terminalItem);
    rowsModel->setItem(row, 1, simItem);
    rowsModel->setItem(row, 2, commentItem);
}

void RentalForm::on_btnDeleteRow_clicked()
{
    int row = ui->tableView->currentIndex().row();
    if (row >= 0) {
        rowsModel->removeRow(row);
    }
}

void RentalForm::on_btnPost_clicked()
{
    if (rowsModel->rowCount() == 0) {
        QMessageBox::warning(this, "Внимание", "Добавьте хотя бы одну строку!");
        return;
    }

    int clientId = ui->comboBoxClient->currentData().toInt();
    if (clientId == 0) {
        QMessageBox::warning(this, "Внимание", "Выберите клиента!");
        return;
    }

    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    if (!db.transaction()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось начать транзакцию");
        return;
    }

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
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД", "Не удалось обновить шапку: " + query.lastError().text());
            return;
        }

        docId = m_editDocId;

        QSqlQuery deleteQuery(db);
        deleteQuery.prepare("DELETE FROM tblrentaldetails WHERE rentaldocid = :id");
        deleteQuery.bindValue(":id", docId);
        if (!deleteQuery.exec()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД",
                                  "Не удалось удалить старые строки: " + deleteQuery.lastError().text());
            return;
        }
    } else {
        if (ui->lineEditNumber->text().trimmed().isEmpty()) {
            QString num = DatabaseManager::instance().generateDocNumber("rental");
            if (num.isEmpty()) {
                db.rollback();
                QMessageBox::critical(this, "Ошибка БД", "Не удалось сгенерировать номер документа.");
                return;
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
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД", "Не удалось создать шапку: " + query.lastError().text());
            return;
        }
        docId = query.value(0).toInt();
    }

    // Терминалы, числившиеся в документе до редактирования
    QSet<int> previousTerminals;
    const QList<int> originalKeys = m_originalDetails.keys();
    for (int k : originalKeys)
        previousTerminals.insert(k);

    for (int i = 0; i < rowsModel->rowCount(); ++i) {
        int terminalId = rowsModel->data(rowsModel->index(i, 0), Qt::UserRole).toInt();
        int simId = rowsModel->data(rowsModel->index(i, 1), Qt::UserRole).toInt();
        QString simNumber = rowsModel->data(rowsModel->index(i, 1), Qt::DisplayRole).toString().trimmed();
        QString comment = rowsModel->data(rowsModel->index(i, 2), Qt::DisplayRole).toString();

        if (terminalId <= 0) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка", QString("Строка %1: выберите терминал.").arg(i + 1));
            return;
        }

        bool wasInDoc = previousTerminals.contains(terminalId);
        int originalSimId = m_editMode ? m_originalDetails.value(terminalId, 0) : 0;

        // Введён новый номер SIM — создаём карточку в справочнике (или берём
        // существующую с таким же номером)
        if (simId == 0 && !simNumber.isEmpty()) {
            if (simNumber.length() > 19) {
                db.rollback();
                QMessageBox::critical(
                    this, "Ошибка",
                    QString("Номер SIM-карты «%1» слишком длинный (макс. 19 символов).").arg(simNumber));
                return;
            }

            QSqlQuery findSim(db);
            findSim.prepare("SELECT simcardid, status FROM tblsimcards WHERE simnumber = :n");
            findSim.bindValue(":n", simNumber);
            if (findSim.exec() && findSim.next()) {
                simId = findSim.value(0).toInt();
                if (findSim.value(1).toInt() != 0) {
                    db.rollback();
                    QMessageBox::critical(this, "Ошибка", QString("SIM-карта %1 уже занята!").arg(simNumber));
                    return;
                }
            } else {
                QSqlQuery insertSim(db);
                insertSim.prepare("INSERT INTO tblsimcards (simnumber, status) VALUES (:n, 0) RETURNING simcardid");
                insertSim.bindValue(":n", simNumber);
                if (!insertSim.exec() || !insertSim.next()) {
                    db.rollback();
                    QMessageBox::critical(this, "Ошибка БД",
                                          QString("Не удалось создать SIM-карту %1: %2")
                                              .arg(simNumber)
                                              .arg(insertSim.lastError().text()));
                    return;
                }
                simId = insertSim.value(0).toInt();
                DatabaseManager::instance().logAction("INSERT", "tblsimcards", simId, QString(), QString(),
                                                      QString("simnumber=%1").arg(simNumber));
                rowsModel->setData(rowsModel->index(i, 1), simId, Qt::UserRole);
            }
        }

        // Блокируем терминал и проверяем его состояние
        QSqlQuery checkQuery(db);
        checkQuery.prepare("SELECT status FROM tblterminals WHERE terminalid = :id FOR UPDATE NOWAIT");
        checkQuery.bindValue(":id", terminalId);

        if (!checkQuery.exec() || !checkQuery.next()) {
            db.rollback();
            QMessageBox::critical(
                this, "Ошибка",
                QString("Не удалось заблокировать терминал %1. Возможно, он уже сдан в аренду.").arg(terminalId));
            return;
        }
        int status = checkQuery.value(0).toInt();

        if (!wasInDoc) {
            // Новый терминал в документе: должен быть свободен
            if (status != 0) {
                db.rollback();
                QMessageBox::critical(this, "Ошибка", QString("Терминал %1 больше не свободен!").arg(terminalId));
                return;
            }
        } else if (status != 1) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка",
                                  QString("Терминал %1 уже не числится в аренде по этому документу.").arg(terminalId));
            return;
        }

        bool simChanged = simId != originalSimId;

        // Освобождаем прежнюю SIM, если привязка в строке изменилась
        if (wasInDoc && simChanged && originalSimId > 0) {
            QSqlQuery freeOld(db);
            freeOld.prepare("UPDATE tblsimcards SET status = 0 WHERE simcardid = :id");
            freeOld.bindValue(":id", originalSimId);
            if (!freeOld.exec()) {
                db.rollback();
                QMessageBox::critical(this, "Ошибка БД",
                                      QString("Не удалось освободить SIM-карту %1: %2")
                                          .arg(originalSimId)
                                          .arg(freeOld.lastError().text()));
                return;
            }
        }

        // Занимаем новую SIM (новый терминал или замена SIM в существующей строке)
        if (simId > 0 && simChanged) {
            QSqlQuery simLock(db);
            simLock.prepare("SELECT status FROM tblsimcards WHERE simcardid = :id AND status = 0 FOR UPDATE NOWAIT");
            simLock.bindValue(":id", simId);
            if (!simLock.exec() || !simLock.next()) {
                db.rollback();
                QMessageBox::critical(this, "Ошибка", QString("SIM-карта %1 уже занята другим терминалом!").arg(simId));
                return;
            }

            QSqlQuery simQuery(db);
            simQuery.prepare("UPDATE tblsimcards SET status = 1 WHERE simcardid = :id");
            simQuery.bindValue(":id", simId);
            if (!simQuery.exec()) {
                db.rollback();
                QMessageBox::critical(
                    this, "Ошибка БД",
                    QString("Не удалось обновить SIM-карту %1: %2").arg(simId).arg(simQuery.lastError().text()));
                return;
            }
        }

        if (!wasInDoc) {
            // Новый терминал — переводим в аренду и привязываем SIM
            QSqlQuery updateQuery(db);
            updateQuery.prepare("UPDATE tblterminals SET status = 1, currentsimcardid = :simid WHERE terminalid = :id");
            updateQuery.bindValue(":id", terminalId);
            updateQuery.bindValue(":simid", simId > 0 ? QVariant(simId) : QVariant());
            if (!updateQuery.exec()) {
                db.rollback();
                QMessageBox::critical(
                    this, "Ошибка БД",
                    QString("Не удалось обновить терминал %1: %2").arg(terminalId).arg(updateQuery.lastError().text()));
                return;
            }
        } else if (simChanged) {
            // Существующий терминал — обновляем только привязку SIM
            QSqlQuery updateQuery(db);
            updateQuery.prepare("UPDATE tblterminals SET currentsimcardid = :simid WHERE terminalid = :id");
            updateQuery.bindValue(":id", terminalId);
            updateQuery.bindValue(":simid", simId > 0 ? QVariant(simId) : QVariant());
            if (!updateQuery.exec()) {
                db.rollback();
                QMessageBox::critical(
                    this, "Ошибка БД",
                    QString("Не удалось обновить терминал %1: %2").arg(terminalId).arg(updateQuery.lastError().text()));
                return;
            }
        }

        QSqlQuery detailQuery(db);
        detailQuery.prepare("INSERT INTO tblrentaldetails (rentaldocid, terminalid, simcardid, comment) "
                            "VALUES (:did, :tid, :sid, :comm)");
        detailQuery.bindValue(":did", docId);
        detailQuery.bindValue(":tid", terminalId);
        detailQuery.bindValue(":sid", simId > 0 ? QVariant(simId) : QVariant());
        detailQuery.bindValue(":comm", comment);

        if (!detailQuery.exec()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД", "Ошибка связи: " + detailQuery.lastError().text());
            return;
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
            lockQuery.prepare(
                "SELECT status, currentsimcardid FROM tblterminals WHERE terminalid = :id FOR UPDATE NOWAIT");
            lockQuery.bindValue(":id", tid);
            if (!lockQuery.exec() || !lockQuery.next())
                continue;

            int tStatus = lockQuery.value(0).toInt();
            int tSim = lockQuery.value(1).toInt();
            if (tStatus != 1)
                continue;

            if (tSim > 0) {
                QSqlQuery freeSim(db);
                freeSim.prepare("UPDATE tblsimcards SET status = 0 WHERE simcardid = :id");
                freeSim.bindValue(":id", tSim);
                if (!freeSim.exec()) {
                    db.rollback();
                    QMessageBox::critical(
                        this, "Ошибка БД",
                        QString("Не удалось освободить SIM-карту %1: %2").arg(tSim).arg(freeSim.lastError().text()));
                    return;
                }
            }

            QSqlQuery upd(db);
            upd.prepare("UPDATE tblterminals SET status = 0, currentsimcardid = NULL WHERE terminalid = :id");
            upd.bindValue(":id", tid);
            if (!upd.exec()) {
                db.rollback();
                QMessageBox::critical(
                    this, "Ошибка БД",
                    QString("Не удалось освободить терминал %1: %2").arg(tid).arg(upd.lastError().text()));
                return;
            }
        }
    }

    if (!db.commit()) {
        db.rollback();
        QMessageBox::critical(this, "Ошибка", "Не удалось зафиксировать транзакцию");
    } else {
        DatabaseManager::instance().logAction("POST", "tblrentaldocs", docId);

        isPosted = true;
        QMessageBox::information(this, "Успех", "Документ успешно проведен!");
        DatabaseManager::instance().notifyDataChanged();
        this->close();
    }
}

void RentalForm::onTableViewDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
    // Когда данные изменились, обновляем отображение
    Q_UNUSED(bottomRight);

    int row = topLeft.row();
    int column = topLeft.column();

    // Если изменилась колонка терминала или SIM
    if (column == 0 || column == 1) {
        // Принудительно обновляем отображение
        QModelIndex index = rowsModel->index(row, column);
        Q_UNUSED(index);
    }
}

void RentalForm::on_btnClose_clicked()
{
    close();
}

void RentalForm::on_btnPrintAct_clicked()
{
    int clientId = ui->comboBoxClient->currentData().toInt();
    if (clientId == 0) {
        QMessageBox::warning(this, "Внимание", "Сначала выберите клиента!");
        return;
    }

    if (!isPosted) {
        QMessageBox::StandardButton btn = QMessageBox::warning(this, "Внимание",
                                                               "Акт будет распечатан до проведения документа. "
                                                               "После проведения данные могут измениться.\n\n"
                                                               "Распечатать как черновик?",
                                                               QMessageBox::Yes | QMessageBox::No);
        if (btn != QMessageBox::Yes)
            return;
    }

    // Получаем данные клиента
    QSqlQuery clientQuery(DatabaseManager::instance().getDatabase());
    clientQuery.prepare("SELECT clientname, inn, address FROM tblclients WHERE clientid = :id");
    clientQuery.bindValue(":id", clientId);

    QString clientName, clientInn, clientAddress;
    if (clientQuery.exec() && clientQuery.next()) {
        clientName = clientQuery.value(0).toString();
        clientInn = clientQuery.value(1).toString();
        clientAddress = clientQuery.value(2).toString();
    }

    // Формируем HTML акта
    QString html = "<html><head><meta charset='utf-8'>"
                   "<style>"
                   "body { font-family: 'Times New Roman', serif; font-size: 14px; }"
                   "h2 { text-align: center; }"
                   "table { border-collapse: collapse; width: 100%; margin-top: 20px; }"
                   "th, td { border: 1px solid black; padding: 6px; text-align: left; }"
                   "th { background-color: #f0f0f0; }"
                   ".signature { margin-top: 50px; display: flex; justify-content: space-between; }"
                   ".signature div { width: 45%; }"
                   "</style></head><body>";

    html += "<h2>АКТ ПРИЁМА-ПЕРЕДАЧИ ТЕРМИНАЛОВ № " + ui->lineEditNumber->text().toHtmlEscaped() + "</h2>";
    html += "<p>от " + ui->dateEdit->date().toString("dd.MM.yyyy") + " г.</p>";
    html += "<p><b>Арендодатель:</b> ООО «POC Terminal»</p>";
    html += "<p><b>Арендатор:</b> " + clientName.toHtmlEscaped();
    if (!clientInn.isEmpty())
        html += " (ИНН: " + clientInn.toHtmlEscaped() + ")";
    if (!clientAddress.isEmpty())
        html += ", адрес: " + clientAddress.toHtmlEscaped();
    html += "</p>";
    html +=
        "<p>Настоящий акт составлен о том, что Арендодатель передал, а Арендатор принял следующие POC-терминалы:</p>";

    html += "<table><tr><th>№</th><th>Серийный номер</th><th>IMEI 1</th><th>SIM-карта</th></tr>";

    // Собираем данные из таблицы
    int num = 1;
    for (int i = 0; i < rowsModel->rowCount(); ++i) {
        int termId = rowsModel->data(rowsModel->index(i, 0), Qt::UserRole).toInt();
        int simId = rowsModel->data(rowsModel->index(i, 1), Qt::UserRole).toInt();

        if (termId == 0)
            continue;

        // Получаем серийный номер и IMEI
        QSqlQuery termQuery(DatabaseManager::instance().getDatabase());
        termQuery.prepare("SELECT serialnumber, imei1 FROM tblterminals WHERE terminalid = :id");
        termQuery.bindValue(":id", termId);
        QString serial, imei;
        if (termQuery.exec() && termQuery.next()) {
            serial = termQuery.value(0).toString();
            imei = termQuery.value(1).toString();
        }

        // Получаем номер SIM
        QString simNumber;
        if (simId > 0) {
            QSqlQuery simQuery(DatabaseManager::instance().getDatabase());
            simQuery.prepare("SELECT simnumber FROM tblsimcards WHERE simcardid = :id");
            simQuery.bindValue(":id", simId);
            if (simQuery.exec() && simQuery.next()) {
                simNumber = simQuery.value(0).toString();
            }
        }

        html += "<tr><td>" + QString::number(num++) +
                "</td>"
                "<td>" +
                serial.toHtmlEscaped() +
                "</td>"
                "<td>" +
                imei.toHtmlEscaped() +
                "</td>"
                "<td>" +
                simNumber.toHtmlEscaped() + "</td></tr>";
    }
    html += "</table>";

    html += "<div class='signature'>"
            "<div><p>Передал (Арендодатель):</p><p>________________ / ____________</p></div>"
            "<div><p>Принял (Арендатор):</p><p>________________ / ____________</p></div>"
            "</div>";
    html += "</body></html>";

    // Печать или сохранение в PDF
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog printDialog(&printer, this);
    if (printDialog.exec() == QDialog::Accepted) {
        QTextDocument doc;
        doc.setHtml(html);
        doc.print(&printer);
    }
}