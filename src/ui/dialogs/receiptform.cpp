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

    // 1. Создаем шапку документа
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
    int docId = query.value(0).toInt();

    // 2. Обрабатываем строки
    for (int i = 0; i < rowsModel->rowCount(); ++i) {
        QString serial = rowsModel->data(rowsModel->index(i, 0)).toString();
        int modelId = rowsModel->data(rowsModel->index(i, 1)).toInt();
        QString imei1 = rowsModel->data(rowsModel->index(i, 2)).toString();
        QString imei2 = rowsModel->data(rowsModel->index(i, 3)).toString();

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

        if (!imei1.isEmpty() && !Validator::validateIMEI(imei1)) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка",
                QString("Строка %1: IMEI 1 должен содержать ровно 15 цифр.").arg(i + 1));
            return;
        }

        if (!imei2.isEmpty() && !Validator::validateIMEI(imei2)) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка",
                QString("Строка %1: IMEI 2 должен содержать ровно 15 цифр.").arg(i + 1));
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