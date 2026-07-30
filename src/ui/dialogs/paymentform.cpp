#include "paymentform.h"
#include "ui_paymentform.h"
#include "database/databasemanager.h"
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDatabase>
#include <QDateTime>
#include <QTime>
#include <QStandardItemModel>
#include <QDebug>
#include <QPrinter>
#include <QPrintDialog>
#include <QTextDocument>

PaymentForm::PaymentForm(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PaymentForm)
{
    ui->setupUi(this);
    setWindowTitle("Документ: Отметка оплаты за аренду");
    resize(500, 450);

    ui->dateEdit->setDate(QDate::currentDate());
    loadClients();
    loadMonths();
    loadYears();

    ui->doubleSpinBoxAmount->setValue(0.00);
    ui->doubleSpinBoxAmount->setDecimals(2);
    ui->doubleSpinBoxAmount->setMinimum(0.00);
    ui->doubleSpinBoxAmount->setMaximum(999999.99);

    // Настраиваем список документов аренды
    QStandardItemModel* listModel = new QStandardItemModel(this);
    ui->listViewRentals->setModel(listModel);
    ui->listViewRentals->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

PaymentForm::~PaymentForm()
{
    delete ui;
}

void PaymentForm::loadClients()
{
    if (!DatabaseManager::instance().isConnected()) {
        qDebug() << "[PaymentForm] База данных не подключена";
        return;
    }

    ui->comboBoxClient->clear();
    ui->comboBoxClient->addItem("-- Выберите клиента --", 0);

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.exec("SELECT clientid, clientname FROM tblclients ORDER BY clientname");
    
    while (query.next()) {
        ui->comboBoxClient->addItem(query.value(1).toString(), query.value(0).toInt());
    }
}

void PaymentForm::loadForEdit(int paymentId)
{
    m_editMode = true;
    m_editPaymentId = paymentId;

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("SELECT clientid, paymentdate, periodmonth, periodyear, amount, comment "
                  "FROM tblpayments WHERE paymentid = :id");
    query.bindValue(":id", paymentId);
    if (!query.exec() || !query.next()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось загрузить платёж");
        reject();
        return;
    }

    int clientId = query.value(0).toInt();
    QDateTime docDate = query.value(1).toDateTime();
    int month = query.value(2).toInt();
    int year = query.value(3).toInt();
    double amount = query.value(4).toDouble();
    QString comment = query.value(5).toString();

    ui->dateEdit->setDate(docDate.date());
    ui->spinBoxYear->setValue(year);
    ui->doubleSpinBoxAmount->setValue(amount);
    ui->textEditComment->setText(comment);

    // Устанавливаем клиента
    for (int i = 0; i < ui->comboBoxClient->count(); ++i) {
        if (ui->comboBoxClient->itemData(i).toInt() == clientId) {
            ui->comboBoxClient->setCurrentIndex(i);
            break;
        }
    }

    // Устанавливаем месяц
    for (int i = 0; i < ui->comboBoxMonth->count(); ++i) {
        if (ui->comboBoxMonth->itemData(i).toInt() == month) {
            ui->comboBoxMonth->setCurrentIndex(i);
            break;
        }
    }

    // Загружаем привязанные документы аренды и отмечаем их
    QSqlQuery linkQuery(DatabaseManager::instance().getDatabase());
    linkQuery.prepare("SELECT rentaldocid FROM tblpayment_rental_links WHERE paymentid = :id");
    linkQuery.bindValue(":id", paymentId);
    linkQuery.exec();

    QSet<int> linkedRentalIds;
    while (linkQuery.next()) {
        linkedRentalIds.insert(linkQuery.value(0).toInt());
    }

    QStandardItemModel* listModel = qobject_cast<QStandardItemModel*>(ui->listViewRentals->model());
    if (listModel) {
        for (int i = 0; i < listModel->rowCount(); ++i) {
            QStandardItem* item = listModel->item(i);
            if (item && linkedRentalIds.contains(item->data(Qt::UserRole).toInt())) {
                item->setCheckState(Qt::Checked);
            }
        }
    }

    setWindowTitle(QString("Редактирование оплаты ID %1").arg(paymentId));
}

void PaymentForm::loadMonths()
{
    ui->comboBoxMonth->clear();
    QStringList monthNames = {
        "Январь", "Февраль", "Март", "Апрель", "Май", "Июнь",
        "Июль", "Август", "Сентябрь", "Октябрь", "Ноябрь", "Декабрь"
    };
    for (int i = 0; i < 12; ++i) {
        ui->comboBoxMonth->addItem(monthNames[i], i + 1);
    }
    ui->comboBoxMonth->setCurrentIndex(QDate::currentDate().month() - 1);
}

void PaymentForm::loadYears()
{
    int currentYear = QDate::currentDate().year();
    ui->spinBoxYear->setMinimum(currentYear - 2);
    ui->spinBoxYear->setMaximum(currentYear + 2);
    ui->spinBoxYear->setValue(currentYear);
}

void PaymentForm::loadRentalDocsForClient(int clientId)
{
    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(ui->listViewRentals->model());
    if (!model) {
        qDebug() << "[PaymentForm] Модель listViewRentals не инициализирована";
        return;
    }
    model->removeRows(0, model->rowCount());

    if (clientId == 0) return;

    if (!DatabaseManager::instance().isConnected()) {
        qDebug() << "[PaymentForm] База данных не подключена";
        return;
    }

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("SELECT rentaldocid, docnumber, docdate FROM tblrentaldocs "
                  "WHERE clientid = :cid ORDER BY docdate DESC");
    query.bindValue(":cid", clientId);

    if (!query.exec()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось загрузить документы аренды: " + query.lastError().text());
        return;
    }

    while (query.next()) {
        QString displayText = QString("%1 от %2")
            .arg(query.value(1).toString())
            .arg(query.value(2).toDateTime().toString("dd.MM.yyyy"));
        
        QStandardItem* item = new QStandardItem(displayText);
        item->setData(query.value(0).toInt(), Qt::UserRole);
        item->setCheckable(true);
        item->setCheckState(Qt::Unchecked);
        
        model->appendRow(item);
    }
}

void PaymentForm::on_comboBoxClient_currentIndexChanged(int index)
{
    Q_UNUSED(index);
    int clientId = ui->comboBoxClient->currentData().toInt();
    loadRentalDocsForClient(clientId);
}

bool PaymentForm::checkExistingPayment(int clientId, int month, int year)
{
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("SELECT paymentid FROM tblpayments "
                  "WHERE clientid = :cid AND periodmonth = :month AND periodyear = :year");
    query.bindValue(":cid", clientId);
    query.bindValue(":month", month);
    query.bindValue(":year", year);
    
    return (query.exec() && query.next());
}

void PaymentForm::on_btnSave_clicked()
{
    int clientId = ui->comboBoxClient->currentData().toInt();
    int month = ui->comboBoxMonth->currentData().toInt();
    int year = ui->spinBoxYear->value();
    double amount = ui->doubleSpinBoxAmount->value();
    
    if (clientId == 0) {
        QMessageBox::warning(this, "Внимание", "Выберите клиента!");
        return;
    }
    if (amount <= 0) {
        QMessageBox::warning(this, "Внимание", "Сумма оплаты должна быть больше нуля!");
        return;
    }

    // Собираем выбранные документы аренды
    QList<int> selectedRentalIds;
    QStandardItemModel* listModel = qobject_cast<QStandardItemModel*>(ui->listViewRentals->model());
    if (listModel) {
        for (int i = 0; i < listModel->rowCount(); ++i) {
            QStandardItem* item = listModel->item(i);
            if (item && item->checkState() == Qt::Checked) {
                selectedRentalIds.append(item->data(Qt::UserRole).toInt());
            }
        }
    }

    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    if (!db.transaction()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось начать транзакцию");
        return;
    }

    int paymentId = m_editPaymentId;

    if (m_editMode) {
        // Режим редактирования — UPDATE существующего платежа
        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE tblpayments SET clientid = :cid, paymentdate = :date, "
                           "periodmonth = :month, periodyear = :year, amount = :amount, comment = :comment "
                           "WHERE paymentid = :id");
        updateQuery.bindValue(":cid", clientId);
        updateQuery.bindValue(":date", QDateTime(ui->dateEdit->date(), QTime::currentTime()));
        updateQuery.bindValue(":month", month);
        updateQuery.bindValue(":year", year);
        updateQuery.bindValue(":amount", amount);
        updateQuery.bindValue(":comment", ui->textEditComment->toPlainText());
        updateQuery.bindValue(":id", paymentId);

        if (!updateQuery.exec()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД", "Не удалось обновить платёж: " + updateQuery.lastError().text());
            return;
        }

        // Удаляем старые связи
        QSqlQuery deleteLinks(db);
        deleteLinks.prepare("DELETE FROM tblpayment_rental_links WHERE paymentid = :id");
        deleteLinks.bindValue(":id", paymentId);
        if (!deleteLinks.exec()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД", "Не удалось обновить связи: " + deleteLinks.lastError().text());
            return;
        }
    } else {
        // Режим создания — проверка дубликата
        if (checkExistingPayment(clientId, month, year)) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "Подтверждение",
                QString("Оплата за %1 %2 года уже существует. Заменить её (включая привязанные документы)?")
                    .arg(ui->comboBoxMonth->currentText(), QString::number(year)),
                QMessageBox::Yes | QMessageBox::No);
            
            if (reply != QMessageBox::Yes) {
                db.rollback();
                return;
            }
            
            QSqlQuery deleteQuery(db);
            deleteQuery.prepare("DELETE FROM tblpayments "
                                "WHERE clientid = :cid AND periodmonth = :month AND periodyear = :year");
            deleteQuery.bindValue(":cid", clientId);
            deleteQuery.bindValue(":month", month);
            deleteQuery.bindValue(":year", year);
            
            if (!deleteQuery.exec()) {
                db.rollback();
                QMessageBox::critical(this, "Ошибка БД", "Не удалось удалить старую запись: " + deleteQuery.lastError().text());
                return;
            }
        }

        // Вставляем новую оплату
        QSqlQuery query(db);
        query.prepare("INSERT INTO tblpayments (clientid, paymentdate, periodmonth, periodyear, amount, comment) "
                      "VALUES (:cid, :date, :month, :year, :amount, :comment) RETURNING paymentid");
        query.bindValue(":cid", clientId);
        query.bindValue(":date", QDateTime(ui->dateEdit->date(), QTime::currentTime()));
        query.bindValue(":month", month);
        query.bindValue(":year", year);
        query.bindValue(":amount", amount);
        query.bindValue(":comment", ui->textEditComment->toPlainText());

        if (!query.exec() || !query.next()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД", "Не удалось сохранить оплату: " + query.lastError().text());
            return;
        }
        paymentId = query.value(0).toInt();
    }
    for (int rentalId : selectedRentalIds) {
        QSqlQuery linkQuery(db);
        linkQuery.prepare("INSERT INTO tblpayment_rental_links (paymentid, rentaldocid) VALUES (:pid, :rid)");
        linkQuery.bindValue(":pid", paymentId);
        linkQuery.bindValue(":rid", rentalId);
        
        if (!linkQuery.exec()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД", "Не удалось создать связь: " + linkQuery.lastError().text());
            return;
        }
    }

    // 4. Фиксируем
    if (!db.commit()) {
        db.rollback();
        QMessageBox::critical(this, "Ошибка", "Не удалось зафиксировать транзакцию");
    } else {
        // Логирование действия
        DatabaseManager::instance().logAction("POST", "tblpayments", paymentId);
        
        QMessageBox::information(this, "Успех", "Оплата и связи успешно сохранены!");
        DatabaseManager::instance().notifyDataChanged();
        this->close();
    }
}

void PaymentForm::on_btnPrint_clicked()
{
    int clientId = ui->comboBoxClient->currentData().toInt();
    if (clientId == 0) {
        QMessageBox::warning(this, "Внимание", "Сначала выберите клиента!");
        return;
    }

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("SELECT clientname, inn FROM tblclients WHERE clientid = :id");
    query.bindValue(":id", clientId);
    QString clientName, clientInn;
    if (query.exec() && query.next()) {
        clientName = query.value(0).toString();
        clientInn = query.value(1).toString();
    }

    QStringList monthNames = {"", "Январь", "Февраль", "Март", "Апрель", "Май", "Июнь",
                              "Июль", "Август", "Сентябрь", "Октябрь", "Ноябрь", "Декабрь"};
    int month = ui->comboBoxMonth->currentIndex();
    int year = ui->spinBoxYear->value();
    double amount = ui->doubleSpinBoxAmount->value();

    QString html = "<html><head><meta charset='utf-8'>"
                   "<style>"
                   "body { font-family: 'Times New Roman', serif; font-size: 14px; }"
                   "h2 { text-align: center; }"
                   "</style></head><body>";

    html += "<h2>КВИТАНЦИЯ ОБ ОПЛАТЕ</h2>";
    html += "<p><b>Платёж №</b> " + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + "</p>";
    html += "<p><b>Дата:</b> " + ui->dateEdit->date().toString("dd.MM.yyyy") + "</p>";
    html += "<p><b>Плательщик:</b> " + clientName;
    if (!clientInn.isEmpty()) html += " (ИНН: " + clientInn + ")";
    html += "</p>";
    html += "<p><b>Получатель:</b> ООО «POC Terminal»</p>";
    html += "<p><b>Период оплаты:</b> " + monthNames.value(month) + " " + QString::number(year) + "</p>";
    html += "<hr>";
    html += "<p style='font-size: 16px;'><b>Сумма:</b> " + QString::number(amount, 'f', 2) + " руб.</p>";
    html += "<p style='font-size: 13px; color: #555;'>Сумма прописью: ...</p>";
    html += "<hr>";

    QString comment = ui->textEditComment->toPlainText().trimmed();
    if (!comment.isEmpty())
        html += "<p><b>Комментарий:</b> " + comment + "</p>";

    html += "<div style='margin-top: 50px; display: flex; justify-content: space-between;'>"
            "<div><p>Кассир: ________________</p></div>"
            "<div><p>Плательщик: ________________</p></div>"
            "</div>";
    html += "</body></html>";

    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog printDialog(&printer, this);
    if (printDialog.exec() == QDialog::Accepted) {
        QTextDocument doc;
        doc.setHtml(html);
        doc.print(&printer);
    }
}

void PaymentForm::on_btnClose_clicked()
{
    close();
}