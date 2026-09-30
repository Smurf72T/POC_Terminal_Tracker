#include "paymentform.h"
#include "ui_paymentform.h"
#include "database/databasemanager.h"
#include "database/repositories/documentrepository.h"
#include "database/repositories/paymentrepository.h"
#include "services/postactionlogger.h"
#include "ui/base/printservice.h"
#include "ui/base/transactionguard.h"
#include <QHeaderView>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDatabase>
#include <QDateTime>
#include <QTime>
#include <QStandardItemModel>
#include <QDebug>
#include "utils/logging.h"

PaymentForm::PaymentForm(QWidget* parent) : ClientDocumentDialog(parent), ui(new Ui::PaymentForm)
{
    ui->setupUi(this);
    setWindowTitle("Документ: Отметка оплаты за аренду");
    resize(620, 470);

    ui->dateEdit->setDate(QDate::currentDate());
    loadClientsToComboBox(ui->comboBoxClient, true);
    loadMonths();
    loadYears();

    ui->doubleSpinBoxAmount->setValue(0.00);
    ui->doubleSpinBoxAmount->setDecimals(2);
    ui->doubleSpinBoxAmount->setMinimum(0.00);
    ui->doubleSpinBoxAmount->setMaximum(999999.99);

    // Таблица привязки: документ аренды + сумма уже оплаченного по нему
    QStandardItemModel* linkModel = new QStandardItemModel(this);
    linkModel->setHorizontalHeaderLabels({"Документ аренды", "Уже оплачено"});
    ui->tableViewRentals->setModel(linkModel);
    ui->tableViewRentals->verticalHeader()->setVisible(false);
    ui->tableViewRentals->horizontalHeader()->setSectionResizeMode(ColRental, QHeaderView::Stretch);
    ui->tableViewRentals->horizontalHeader()->setSectionResizeMode(ColPaid, QHeaderView::ResizeToContents);
}

PaymentForm::~PaymentForm()
{
    delete ui;
}

QString PaymentForm::docType() const
{
    return "payment";
}

QLineEdit* PaymentForm::headerNumberEdit() const
{
    return nullptr;
}

QDateEdit* PaymentForm::headerDateEdit() const
{
    return ui->dateEdit;
}

QTextEdit* PaymentForm::headerCommentEdit() const
{
    return ui->textEditComment;
}

QTableView* PaymentForm::tableView() const
{
    return nullptr;
}

void PaymentForm::loadSpecificEditData(int docId)
{

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("SELECT clientid, paymentdate, periodmonth, periodyear, amount, comment "
                  "FROM tblpayments WHERE paymentid = :id");
    query.bindValue(":id", docId);
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
    linkQuery.bindValue(":id", docId);
    if (!linkQuery.exec()) {
        QMessageBox::warning(this, "Ошибка БД",
                             "Не удалось загрузить привязанные документы аренды:\n" + linkQuery.lastError().text());
        return;
    }

    QSet<int> linkedRentalIds;
    while (linkQuery.next()) {
        linkedRentalIds.insert(linkQuery.value(0).toInt());
    }

    applyLinkedRentalDocs(linkedRentalIds);

    setWindowTitle(QString("Редактирование оплаты ID %1").arg(docId));
}

void PaymentForm::applyLinkedRentalDocs(const QSet<int>& linkedRentalIds)
{
    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(ui->tableViewRentals->model());
    if (!model)
        return;

    for (int i = 0; i < model->rowCount(); ++i) {
        QStandardItem* item = model->item(i, ColRental);
        if (item && linkedRentalIds.contains(item->data(kRentalIdRole).toInt()))
            item->setCheckState(Qt::Checked);
    }
}

void PaymentForm::loadMonths()
{
    ui->comboBoxMonth->clear();
    QStringList monthNames = {"Январь", "Февраль", "Март",     "Апрель",  "Май",    "Июнь",
                              "Июль",   "Август",  "Сентябрь", "Октябрь", "Ноябрь", "Декабрь"};
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
    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(ui->tableViewRentals->model());
    if (!model) {
        qCWarning(logApp) << "Модель tableViewRentals не инициализирована";
        return;
    }
    model->removeRows(0, model->rowCount());

    if (clientId == 0)
        return;

    if (!DatabaseManager::instance().isConnected()) {
        qCWarning(logApp) << "База данных не подключена";
        return;
    }

    const QSqlDatabase db = DatabaseManager::instance().getDatabase();

    // Сколько уже оплачено по каждому документу (при редактировании — без вклада
    // самого редактируемого платежа).
    const QHash<int, double> paid = PaymentRepository(db).paidByRentalDocs(clientId, m_editMode ? m_editDocId : 0);

    const auto docs = DocumentRepository(db).loadRentalDocumentsByClient(clientId);
    for (const auto& doc : docs) {
        const QString displayText = QString("%1 от %2").arg(doc.docNumber, doc.date.toString("dd.MM.yyyy"));

        QStandardItem* rentalItem = new QStandardItem(displayText);
        rentalItem->setData(doc.id, kRentalIdRole);
        rentalItem->setCheckable(true);
        rentalItem->setCheckState(Qt::Unchecked);

        QStandardItem* paidItem = new QStandardItem(QString::number(paid.value(doc.id, 0.0), 'f', 2));
        paidItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        paidItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        paidItem->setToolTip("Сумма оплат, привязанных к этому документу аренды");

        model->appendRow({rentalItem, paidItem});
    }
}

void PaymentForm::on_comboBoxClient_currentIndexChanged(int index)
{
    Q_UNUSED(index);
    int clientId = ui->comboBoxClient->currentData().toInt();
    loadRentalDocsForClient(clientId);
}

void PaymentForm::on_btnSave_clicked()
{
    executePost();
}


void PaymentForm::onPostSuccess(int docId)
{
    PostActionLogger::log("POST", "tblpayments", docId);

    QMessageBox::information(this, "Успех", "Оплата и связи успешно сохранены!");
    PostActionLogger::notify();
    this->close();
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

    QStringList monthNames = {"",     "Январь", "Февраль",  "Март",    "Апрель", "Май",    "Июнь",
                              "Июль", "Август", "Сентябрь", "Октябрь", "Ноябрь", "Декабрь"};
    int month = ui->comboBoxMonth->currentIndex();
    int year = ui->spinBoxYear->value();
    double amount = ui->doubleSpinBoxAmount->value();

    QString html = PrintService::docHeader();

    html += "<h2>КВИТАНЦИЯ ОБ ОПЛАТЕ</h2>";
    html += "<p><b>Платёж №</b> " + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + "</p>";
    html += "<p><b>Дата:</b> " + ui->dateEdit->date().toString("dd.MM.yyyy") + "</p>";
    html += "<p><b>Плательщик:</b> " + clientName.toHtmlEscaped();
    if (!clientInn.isEmpty())
        html += " (ИНН: " + clientInn.toHtmlEscaped() + ")";
    html += "</p>";
    html += "<p><b>Получатель:</b> ООО «POC Terminal»</p>";
    html +=
        "<p><b>Период оплаты:</b> " + monthNames.value(month).toHtmlEscaped() + " " + QString::number(year) + "</p>";
    html += "<hr>";
    html += "<p style='font-size: 16px;'><b>Сумма:</b> " + QString::number(amount, 'f', 2) + " руб.</p>";
    html += "<p style='font-size: 13px; color: #555;'>Сумма прописью: ...</p>";
    html += "<hr>";

    QString comment = ui->textEditComment->toPlainText().trimmed();
    if (!comment.isEmpty())
        html += "<p><b>Комментарий:</b> " + comment.toHtmlEscaped() + "</p>";

    html += "<div style='margin-top: 50px; display: flex; justify-content: space-between;'>"
            "<div><p>Кассир: ________________</p></div>"
            "<div><p>Плательщик: ________________</p></div>"
            "</div>";
    html += PrintService::docFooter();

    PrintService::printHtml(html, this);
}

void PaymentForm::on_btnClose_clicked()
{
    close();
}
