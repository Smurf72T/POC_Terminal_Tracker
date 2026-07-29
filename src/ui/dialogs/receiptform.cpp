#include "receiptform.h"
#include "ui_receiptform.h"
#include "delegates/comboboxdelegate.h"
#include "database/databasemanager.h"
#include "utils/validator.h"
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QDebug>
#include <QKeyEvent>

ReceiptForm::ReceiptForm(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ReceiptForm)
{
    ui->setupUi(this);
    setWindowTitle("Документ: Поступление терминалов");
    resize(900, 600);

    // Настройка даты (сегодня)
    ui->dateEdit->setDate(QDate::currentDate());

    // Генерация номера
    generateDocNumber();

    // Настройка модели для табличной части
    rowsModel = new QStandardItemModel(0, 4, this); // 4 колонки
    rowsModel->setHorizontalHeaderLabels({"Серийный номер", "Модель (ID)", "IMEI 1", "IMEI 2"});
    ui->tableView->setModel(rowsModel);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);

    // Загружаем модели для выпадающего списка
    loadModelsToDelegate();

    // F9 для дублирования строки
    ui->tableView->installEventFilter(this);
}

ReceiptForm::~ReceiptForm()
{
    delete ui;
}

void ReceiptForm::loadModelsToDelegate()
{
    m_models.clear();
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.exec("SELECT modelid, modelname FROM tblmodels ORDER BY modelname");

    while (query.next()) {
        m_models.append(qMakePair(query.value(0).toInt(), query.value(1).toString()));
    }

    // Устанавливаем делегат на колонку 1 (Модель)
    ui->tableView->setItemDelegateForColumn(1, new ComboBoxDelegate(m_models, this));
}

void ReceiptForm::generateDocNumber()
{
    QString number = DatabaseManager::instance().generateDocNumber("receipt");
    if (!number.isEmpty()) {
        ui->lineEditNumber->setText(number);
    } else {
        ui->lineEditNumber->setText("ПП-00001");
    }
}

void ReceiptForm::loadForEdit(int docId)
{
    m_editMode = true;
    m_editDocId = docId;

    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    QSqlQuery query(db);

    // Load header
    query.prepare("SELECT docnumber, docdate, comments FROM tblreceiptdocs WHERE receiptdocid = :id");
    query.bindValue(":id", docId);
    if (query.exec() && query.next()) {
        QString docnumber = query.value(0).toString();
        QDateTime docdate = query.value(1).toDateTime();
        QString comments = query.value(2).toString();

        ui->lineEditNumber->setText(docnumber);
        ui->lineEditNumber->setReadOnly(true);
        ui->dateEdit->setDate(docdate.date());
        ui->textEditComment->setText(comments);
    }

    // Load details
    QSqlQuery detailQuery(db);
    detailQuery.prepare("SELECT t.serialnumber, t.modelid, t.imei1, t.imei2 "
                        "FROM tblreceiptdetails rd "
                        "JOIN tblterminals t ON rd.terminalid = t.terminalid "
                        "WHERE rd.receiptdocid = :id");
    detailQuery.bindValue(":id", docId);
    if (detailQuery.exec()) {
        while (detailQuery.next()) {
            int row = rowsModel->rowCount();
            rowsModel->insertRow(row);

            QString serial = detailQuery.value(0).toString();
            int modelId = detailQuery.value(1).toInt();
            QString imei1 = detailQuery.value(2).toString();
            QString imei2 = detailQuery.value(3).toString();

            rowsModel->setItem(row, 0, new QStandardItem(serial));

            // Find model name from m_models list
            QString modelName;
            for (const auto &pair : m_models) {
                if (pair.first == modelId) {
                    modelName = pair.second;
                    break;
                }
            }
            QStandardItem *modelItem = new QStandardItem();
            modelItem->setText(modelName);
            modelItem->setData(modelId, Qt::UserRole);
            rowsModel->setItem(row, 1, modelItem);

            rowsModel->setItem(row, 2, new QStandardItem(imei1));
            rowsModel->setItem(row, 3, new QStandardItem(imei2));
        }
    }

    setWindowTitle(QString("Редактирование поступления ID %1").arg(docId));
}

void ReceiptForm::on_btnAddRow_clicked()
{
    int row = rowsModel->rowCount();
    rowsModel->insertRow(row);

    // Значения по умолчанию
    rowsModel->setItem(row, 0, new QStandardItem("SN-..."));

    // Модель (ID): берем первый доступный ID из БД
    int defaultModelId = m_models.isEmpty() ? 0 : m_models.first().first;
    QString defaultModelName = m_models.isEmpty() ? QString() : m_models.first().second;

    QStandardItem* modelItem = new QStandardItem();
    modelItem->setText(defaultModelName);
    modelItem->setData(defaultModelId, Qt::UserRole);
    rowsModel->setItem(row, 1, modelItem);

    rowsModel->setItem(row, 2, new QStandardItem("000000000000000"));
    rowsModel->setItem(row, 3, new QStandardItem("000000000000000"));
}

void ReceiptForm::on_btnDeleteRow_clicked()
{
    int row = ui->tableView->currentIndex().row();
    if (row >= 0) {
        rowsModel->removeRow(row);
    }
}

void ReceiptForm::on_btnPost_clicked()
{
    if (rowsModel->rowCount() == 0) {
        QMessageBox::warning(this, "Внимание", "Добавьте хотя бы одну строку!");
        return;
    }

    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    if (!db.transaction()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось начать транзакцию");
        return;
    }

    QSqlQuery query(db);
    int docId;

    // 1. Создаем или обновляем шапку документа
    if (m_editMode) {
        query.prepare("UPDATE tblreceiptdocs SET docdate = :date, comments = :comm WHERE receiptdocid = :id");
        query.bindValue(":date", QDateTime::currentDateTime());
        query.bindValue(":comm", ui->textEditComment->toPlainText());
        query.bindValue(":id", m_editDocId);

        if (!query.exec()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД", "Не удалось обновить шапку: " + query.lastError().text());
            return;
        }
        docId = m_editDocId;
    } else {
        query.prepare("INSERT INTO tblreceiptdocs (docnumber, docdate, comments) "
                      "VALUES (:num, :date, :comm) RETURNING receiptdocid");
        query.bindValue(":num", ui->lineEditNumber->text());
        query.bindValue(":date", QDateTime::currentDateTime());
        query.bindValue(":comm", ui->textEditComment->toPlainText());

        if (!query.exec() || !query.next()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД", "Не удалось создать шапку: " + query.lastError().text());
            return;
        }
        docId = query.value(0).toInt();
    }

    // 2. Обрабатываем строки (только для новых документов)
    if (!m_editMode) {
    for (int i = 0; i < rowsModel->rowCount(); ++i) {
        QString serial = rowsModel->data(rowsModel->index(i, 0)).toString();
        int modelId = rowsModel->data(rowsModel->index(i, 1), Qt::UserRole).toInt();
        QString imei1 = rowsModel->data(rowsModel->index(i, 2)).toString();
        QString imei2 = rowsModel->data(rowsModel->index(i, 3)).toString();

        // Валидация модели
        if (modelId <= 0) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка",
                QString("Строка %1: выберите модель из списка.").arg(i + 1));
            return;
        }

        // Валидация серийного номера
        if (!Validator::validateSerialNotEmpty(serial)) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка", 
                QString("Строка %1: серийный номер должен содержать минимум 3 символа.").arg(i + 1));
            return;
        }

        // Валидация IMEI
        QRegularExpression digitRe("[^\\d]");
        imei1.remove(digitRe);
        imei2.remove(digitRe);

        if (!imei1.isEmpty() && imei1.length() != 15) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка",
                QString("Строка %1: IMEI 1 должен содержать ровно 15 цифр (сейчас: %2, длина: %3)")
                .arg(i + 1).arg(imei1).arg(imei1.length()));
            return;
        }

        if (!imei2.isEmpty() && imei2.length() != 15) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка",
                QString("Строка %1: IMEI 2 должен содержать ровно 15 цифр (сейчас: %2, длина: %3)")
                .arg(i + 1).arg(imei2).arg(imei2.length()));
            return;
        }

        // Вставляем терминал
        QSqlQuery termQuery(db);
        termQuery.prepare("INSERT INTO tblterminals (serialnumber, modelid, imei1, imei2, status) "
                          "VALUES (:sn, :mid, :i1, :i2, 0) RETURNING terminalid");
        termQuery.bindValue(":sn", serial);
        termQuery.bindValue(":mid", modelId);
        termQuery.bindValue(":i1", imei1);
        termQuery.bindValue(":i2", imei2);

        if (!termQuery.exec() || !termQuery.next()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД",
                QString("Ошибка при добавлении терминала %1:\n%2").arg(serial, termQuery.lastError().text()));
            return;
        }
        int termId = termQuery.value(0).toInt();

        // Вставляем связь с документом
        QSqlQuery detailQuery(db);
        detailQuery.prepare("INSERT INTO tblreceiptdetails (receiptdocid, terminalid) VALUES (:did, :tid)");
        detailQuery.bindValue(":did", docId);
        detailQuery.bindValue(":tid", termId);

        if (!detailQuery.exec()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД", "Ошибка связи: " + detailQuery.lastError().text());
            return;
        }
    }

    } // !m_editMode

    // 3. Фиксируем транзакцию
    if (!db.commit()) {
        db.rollback();
        QMessageBox::critical(this, "Ошибка", "Не удалось зафиксировать транзакцию");
    } else {
        // Логирование действия
        DatabaseManager::instance().logAction("POST", "tblreceiptdocs", docId);
        
        QMessageBox::information(this, "Успех", "Документ успешно проведен!");
        DatabaseManager::instance().notifyDataChanged();
        this->close();
    }
}

void ReceiptForm::on_btnClose_clicked()
{
    close();
}

bool ReceiptForm::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->tableView && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_F9) {
            int row = ui->tableView->currentIndex().row();
            if (row < 0) return true;

            int newRow = rowsModel->rowCount();
            rowsModel->insertRow(newRow);

            for (int col = 0; col < rowsModel->columnCount(); ++col) {
                QModelIndex src = rowsModel->index(row, col);
                QStandardItem *newItem = rowsModel->itemFromIndex(src)->clone();
                rowsModel->setItem(newRow, col, newItem);
            }

            ui->tableView->setCurrentIndex(rowsModel->index(newRow, 0));
            return true;
        }
    }
    return QDialog::eventFilter(obj, event);
}