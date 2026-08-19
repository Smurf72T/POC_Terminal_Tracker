#include "returnform.h"
#include "ui_returnform.h"
#include "database/databasemanager.h"
#include "database/repositories/clientrepository.h"
#include "database/repositories/documentrepository.h"
#include "database/repositories/terminalrepository.h"
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QTime>
#include <QDebug>
#include <QPrinter>
#include "utils/logging.h"
#include "services/documentnumbergenerator.h"
#include "services/postactionlogger.h"
#include "ui/base/printservice.h"
#include "ui/base/transactionguard.h"
#include <QHash>

ReturnForm::ReturnForm(QWidget* parent) : ClientDocumentDialog(parent), ui(new Ui::ReturnForm)
{
    ui->setupUi(this);
    setWindowTitle("Документ: Возврат из аренды");
    resize(900, 600);

    ui->dateEdit->setDate(QDate::currentDate());

    // Номер документа генерируется при проведении (не здесь), чтобы не
    // сжигать значения последовательности для отменённых форм.

    // Настройка модели таблицы
    rowsModel->setColumnCount(4);
    rowsModel->setHorizontalHeaderLabels({"Возврат", "Терминал", "SIM (IMEI 1)", "SIM (IMEI 2)"});
    ui->tableView->setModel(rowsModel);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);

    // Загружаем клиентов
    loadClientsToComboBox(ui->comboBoxClient);
}

ReturnForm::~ReturnForm()
{
    delete ui;
}

QString ReturnForm::docType() const
{
    return "return";
}

QLineEdit* ReturnForm::headerNumberEdit() const
{
    return ui->lineEditNumber;
}

QDateEdit* ReturnForm::headerDateEdit() const
{
    return ui->dateEdit;
}

QTextEdit* ReturnForm::headerCommentEdit() const
{
    return ui->textEditComment;
}

QTableView* ReturnForm::tableView() const
{
    return ui->tableView;
}

void ReturnForm::loadRentalDetails(int rentalDocId)
{
    rowsModel->removeRows(0, rowsModel->rowCount());
    if (rentalDocId == 0)
        return;

    // Загружаем строки из документа аренды
    // Показываем все терминалы из документа, даже если они уже возвращены (для истории)
    const auto rows = DocumentRepository(DatabaseManager::instance().getDatabase()).loadRentalRows(rentalDocId);
    for (const auto& row : rows) {
        int r = rowsModel->rowCount();
        rowsModel->insertRow(r);

        // Колонка 0: Чекбокс "Возврат"
        QStandardItem* checkItem = new QStandardItem();
        checkItem->setCheckable(true);
        checkItem->setCheckState(Qt::Unchecked);
        checkItem->setData(row.terminalId, Qt::UserRole); // Храним ID терминала
        rowsModel->setItem(r, 0, checkItem);

        // Колонка 1: Терминал (только для чтения)
        QStandardItem* termItem = new QStandardItem(row.terminalSerialNumber);
        termItem->setEditable(false);
        termItem->setData(row.terminalId, Qt::UserRole);
        rowsModel->setItem(r, 1, termItem);

        // Колонка 2: SIM слота 1 (только для чтения)
        QString simNumber = row.simCardId > 0 ? row.simNumber : QString("Нет SIM");
        QStandardItem* simItem = new QStandardItem(simNumber);
        simItem->setEditable(false);
        rowsModel->setItem(r, 2, simItem);

        // Колонка 3: SIM слота 2 (только для чтения)
        QString sim2Number = row.simCard2Id > 0 ? row.simNumber2 : QString("Нет SIM");
        QStandardItem* sim2Item = new QStandardItem(sim2Number);
        sim2Item->setEditable(false);
        rowsModel->setItem(r, 3, sim2Item);
    }
}

void ReturnForm::on_comboBoxClient_currentIndexChanged(int index)
{
    int clientId = ui->comboBoxClient->itemData(index).toInt();
    loadRentalDocsForClient(ui->comboBoxRentalDoc, clientId);
}

void ReturnForm::on_comboBoxRentalDoc_currentIndexChanged(int index)
{
    int rentalDocId = ui->comboBoxRentalDoc->itemData(index).toInt();
    loadRentalDetails(rentalDocId);
}

void ReturnForm::loadSpecificEditData(int docId)
{
    m_originalReturned.clear();
    m_editRentalDocId = 0;

    const QSqlDatabase& db = DatabaseManager::instance().getDatabase();
    DocumentRepository documents(db);

    const models::DocumentHeader header = documents.loadHeader(DocumentRepository::Return, docId);
    if (header.id == 0) {
        QMessageBox::critical(this, "Ошибка", "Не удалось загрузить документ возврата");
        return;
    }

    ui->lineEditNumber->setText(header.docNumber);
    ui->lineEditNumber->setReadOnly(true);
    ui->dateEdit->setDate(header.date);
    ui->textEditComment->setText(header.comments);
    setWindowTitle(QString("Редактирование возврата ID %1").arg(docId));

    int clientIndex = ui->comboBoxClient->findData(header.clientId);
    if (clientIndex >= 0) {
        ui->comboBoxClient->setCurrentIndex(clientIndex);
    }

    int rentalDocId = documents.rentalDocIdForReturn(docId);
    if (rentalDocId > 0) {
        m_editRentalDocId = rentalDocId;
        int rentalIndex = ui->comboBoxRentalDoc->findData(rentalDocId);
        if (rentalIndex >= 0) {
            ui->comboBoxRentalDoc->setCurrentIndex(rentalIndex);
        }
    }

    const QList<int> returnedTerminals = documents.returnedTerminalIds(docId);
    for (int tid : returnedTerminals)
        m_originalReturned.insert(tid);

    for (int i = 0; i < rowsModel->rowCount(); ++i) {
        QStandardItem* checkItem = rowsModel->item(i, 0);
        if (checkItem && returnedTerminals.contains(checkItem->data(Qt::UserRole).toInt())) {
            checkItem->setCheckState(Qt::Checked);
        }
    }
}

void ReturnForm::on_btnPost_clicked()
{
    executePost();
}


void ReturnForm::onPostSuccess(int docId)
{
    if (m_editMode) {
        PostActionLogger::log("UPDATE", "tblreturndocs", docId);
        QMessageBox::information(this, "Успех", "Возврат успешно обновлен!");
    } else {
        PostActionLogger::log("POST", "tblreturndocs", docId);
        QMessageBox::information(this, "Успех", "Возврат успешно проведен!");
    }
    PostActionLogger::notify();
    this->close();
}
void ReturnForm::on_btnPrint_clicked()
{
    int clientId = ui->comboBoxClient->currentData().toInt();
    if (clientId == 0) {
        QMessageBox::warning(this, "Внимание", "Сначала выберите клиента!");
        return;
    }

    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    if (!m_editMode && ui->lineEditNumber->text().trimmed().isEmpty()) {
        QString num = DocumentNumberGenerator::generate("return", db);
        if (num.isEmpty()) {
            QMessageBox::critical(this, "Ошибка БД", "Не удалось сгенерировать номер документа.");
            return;
        }
        ui->lineEditNumber->setText(num);
    }

    QString html = PrintService::docHeader();

    html += "<h2>АКТ ВОЗВРАТА ТЕРМИНАЛОВ № " + ui->lineEditNumber->text().toHtmlEscaped() + "</h2>";
    html += "<p>от " + ui->dateEdit->date().toString("dd.MM.yyyy") + " г.</p>";
    html += "<p><b>Арендодатель:</b> ООО «POC Terminal»</p>";

    const models::Client client = ClientRepository(db).loadById(clientId);
    QString clientName = client.name;
    QString clientInn = client.inn;
    html += "<p><b>Арендатор:</b> " + clientName.toHtmlEscaped();
    if (!clientInn.isEmpty())
        html += " (ИНН: " + clientInn.toHtmlEscaped() + ")";
    html += "</p>";

    html += "<table><tr><th>№</th><th>Серийный номер</th><th>Модель</th><th>IMEI 1</th></tr>";

    QList<int> terminalIds;
    for (int i = 0; i < rowsModel->rowCount(); ++i) {
        auto* item = rowsModel->item(i, 0);
        if (item && item->data(Qt::UserRole).toInt() > 0)
            terminalIds.append(item->data(Qt::UserRole).toInt());
    }
    QHash<int, models::Terminal> termById = PrintService::loadTerminalsBatch(terminalIds, db);

    for (int i = 0; i < rowsModel->rowCount(); ++i) {
        auto* item = rowsModel->item(i, 0);
        if (!item || item->data(Qt::UserRole).toInt() == 0)
            continue;
        int termId = item->data(Qt::UserRole).toInt();
        QString serial = item->text();

        const models::Terminal term = termById.value(termId);
        QString imei = term.imei1;
        QString modelName = term.modelName.isEmpty() ? QString("—") : term.modelName;

        html += "<tr><td>" + QString::number(i + 1) +
                "</td>"
                "<td>" +
                serial.toHtmlEscaped() +
                "</td>"
                "<td>" +
                modelName.toHtmlEscaped() +
                "</td>"
                "<td>" +
                imei.toHtmlEscaped() + "</td></tr>";
    }
    html += "</table>";

    html += "<div style='margin-top: 40px; display: flex; justify-content: space-between;'>"
            "<div><p>Сдал (Арендатор):</p><p>________________ / ____________</p></div>"
            "<div><p>Принял (Арендодатель):</p><p>________________ / ____________</p></div>"
            "</div>";
    html += PrintService::docFooter();

    PrintService::printHtml(html, this);
}

void ReturnForm::on_btnClose_clicked()
{
    close();
}
