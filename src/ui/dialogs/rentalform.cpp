#include "rentalform.h"
#include "ui_rentalform.h"
#include "delegates/comboboxdelegate.h"
#include "delegates/comboboxmodel.h"
#include "database/databasemanager.h"
#include "database/repositories/clientrepository.h"
#include "database/repositories/documentrepository.h"
#include "database/repositories/simcardrepository.h"
#include "database/repositories/terminalrepository.h"
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QTime>
#include <QDebug>
#include <QSqlRecord>
#include "utils/logging.h"
#include "services/documentnumbergenerator.h"
#include "services/postactionlogger.h"
#include "services/simcardservice.h"
#include "ui/base/printservice.h"
#include "ui/base/transactionguard.h"
#include <QSet>
#include <QHash>
#include <QSqlDatabase>

RentalForm::RentalForm(QWidget* parent) : ClientDocumentDialog(parent), ui(new Ui::RentalForm)
{
    ui->setupUi(this);
    setWindowTitle("Документ: Передача в аренду");
    resize(900, 600);

    // Настройка даты (сегодня)
    ui->dateEdit->setDate(QDate::currentDate());

    // Номер документа генерируется при проведении (не здесь), чтобы не
    // сжигать значения последовательности для отменённых форм.

    // Настройка модели для табличной части
    rowsModel->setColumnCount(4);
    rowsModel->setHorizontalHeaderLabels({"Терминал", "SIM (IMEI 1)", "SIM (IMEI 2)", "Примечание"});
    ui->tableView->setModel(rowsModel);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);

    // Загружаем данные для выпадающих списков
    loadClientsToDelegate(ui->comboBoxClient);
    loadFreeTerminalsToDelegate();
    loadFreeSIMsToDelegate();

    // Подключаем сигнал изменения данных
    connect(rowsModel, &QStandardItemModel::dataChanged, this, &RentalForm::onTableViewDataChanged);
}

RentalForm::~RentalForm()
{
    delete ui;
}

QString RentalForm::docType() const
{
    return "rental";
}

QLineEdit* RentalForm::headerNumberEdit() const
{
    return ui->lineEditNumber;
}

QDateEdit* RentalForm::headerDateEdit() const
{
    return ui->dateEdit;
}

QTextEdit* RentalForm::headerCommentEdit() const
{
    return ui->textEditComment;
}

QTableView* RentalForm::tableView() const
{
    return ui->tableView;
}

void RentalForm::loadFreeTerminalsToDelegate()
{
    // Загрузим только свободные терминалы
    QList<QPair<int, QString>> terminals;
    const auto free = TerminalRepository(DatabaseManager::instance().getDatabase()).loadFreeForSelection();
    for (const auto& t : free)
        terminals.append(qMakePair(t.id, t.serialNumber));

    // Устанавливаем делегат на колонку терминала
    ui->tableView->setItemDelegateForColumn(0, new ComboBoxDelegate(terminals, this));
}

void RentalForm::loadFreeSIMsToDelegate()
{
    QList<QPair<int, QString>> sims;
    const auto free = SimCardRepository(DatabaseManager::instance().getDatabase()).loadFreeForSelection();
    for (const auto& s : free)
        sims.append(qMakePair(s.id, s.number));

    // Устанавливаем делегат на колонку SIM (редактируемый: можно выбрать
    // существующую SIM-карту или ввести новый номер). Обе колонки — слот 1 и слот 2.
    ui->tableView->setItemDelegateForColumn(1, new ComboBoxDelegate(sims, this, true));
    ui->tableView->setItemDelegateForColumn(2, new ComboBoxDelegate(sims, this, true));
}

void RentalForm::loadSpecificEditData(int docId)
{
    m_originalDetails.clear();

    const QSqlDatabase& db = DatabaseManager::instance().getDatabase();
    DocumentRepository documents(db);
    const models::RentalDocument doc = documents.loadRentalDocument(docId);
    if (doc.id == 0) {
        QMessageBox::critical(this, "Ошибка", "Не удалось загрузить документ аренды.");
        return;
    }

    ui->lineEditNumber->setText(doc.docNumber);
    ui->lineEditNumber->setReadOnly(true);
    ui->dateEdit->setDate(doc.date);
    ui->textEditComment->setText(doc.comments);

    for (int i = 0; i < ui->comboBoxClient->count(); ++i) {
        if (ui->comboBoxClient->itemData(i).toInt() == doc.clientId) {
            ui->comboBoxClient->setCurrentIndex(i);
            break;
        }
    }

    const auto rows = documents.loadRentalRows(docId);
    for (const auto& row : rows) {
        m_originalDetails.insert(row.terminalId, qMakePair(row.simCardId, row.simCard2Id));

        int r = rowsModel->rowCount();
        rowsModel->insertRow(r);

        QStandardItem* terminalItem = new QStandardItem();
        terminalItem->setData(row.terminalId, Qt::UserRole);
        terminalItem->setData(row.terminalSerialNumber, Qt::DisplayRole);

        QStandardItem* simItem = new QStandardItem();
        simItem->setData(row.simCardId, Qt::UserRole);
        simItem->setData(row.simCardId > 0 ? row.simNumber : QString(), Qt::DisplayRole);

        QStandardItem* sim2Item = new QStandardItem();
        sim2Item->setData(row.simCard2Id, Qt::UserRole);
        sim2Item->setData(row.simCard2Id > 0 ? row.simNumber2 : QString(), Qt::DisplayRole);

        QStandardItem* commentItem = new QStandardItem(row.comment);

        rowsModel->setItem(r, 0, terminalItem);
        rowsModel->setItem(r, 1, simItem);
        rowsModel->setItem(r, 2, sim2Item);
        rowsModel->setItem(r, 3, commentItem);
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

    QStandardItem* sim2Item = new QStandardItem();
    sim2Item->setData(0, Qt::UserRole);     // ID SIM (слот 2)
    sim2Item->setData("", Qt::DisplayRole); // Текст для отображения

    QStandardItem* commentItem = new QStandardItem("");

    rowsModel->setItem(row, 0, terminalItem);
    rowsModel->setItem(row, 1, simItem);
    rowsModel->setItem(row, 2, sim2Item);
    rowsModel->setItem(row, 3, commentItem);
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
    executePost();
}

void RentalForm::onPostSuccess(int docId)
{
    PostActionLogger::log("POST", "tblrentaldocs", docId);

    isPosted = true;
    QMessageBox::information(this, "Успех", "Документ успешно проведен!");
    PostActionLogger::notify();
    this->close();
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
    const QSqlDatabase& db = DatabaseManager::instance().getDatabase();
    const models::Client client = ClientRepository(db).loadById(clientId);
    QString clientName = client.name;
    QString clientInn = client.inn;
    QString clientAddress = client.address;

    // Формируем HTML акта
    QString html = PrintService::docHeader();
    html += "<style>"
            ".signature { margin-top: 50px; display: flex; justify-content: space-between; }"
            ".signature div { width: 45%; }"
            "</style>";

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

    html += "<table><tr><th>№</th><th>Серийный номер</th><th>IMEI 1</th><th>SIM (IMEI 1)</th>"
            "<th>IMEI 2</th><th>SIM (IMEI 2)</th></tr>";

    // Собираем данные из таблицы (батч-загрузка вместо запросов по каждой строке)
    QList<int> terminalIds;
    QList<int> simIds;
    for (int i = 0; i < rowsModel->rowCount(); ++i) {
        int termId = rowsModel->data(rowsModel->index(i, 0), Qt::UserRole).toInt();
        if (termId == 0)
            continue;
        terminalIds.append(termId);
        int simId = rowsModel->data(rowsModel->index(i, 1), Qt::UserRole).toInt();
        if (simId > 0)
            simIds.append(simId);
        int sim2Id = rowsModel->data(rowsModel->index(i, 2), Qt::UserRole).toInt();
        if (sim2Id > 0)
            simIds.append(sim2Id);
    }

    QHash<int, models::Terminal> termById = PrintService::loadTerminalsBatch(terminalIds, db);
    QHash<int, models::SimCard> simById = PrintService::loadSimsBatch(simIds, db);

    int num = 1;
    for (int i = 0; i < rowsModel->rowCount(); ++i) {
        int termId = rowsModel->data(rowsModel->index(i, 0), Qt::UserRole).toInt();
        int simId = rowsModel->data(rowsModel->index(i, 1), Qt::UserRole).toInt();
        int sim2Id = rowsModel->data(rowsModel->index(i, 2), Qt::UserRole).toInt();

        if (termId == 0)
            continue;

        const models::Terminal term = termById.value(termId);
        QString serial = term.serialNumber;
        QString imei1 = term.imei1;
        QString imei2 = term.imei2;

        QString simNumber;
        if (simId > 0)
            simNumber = simById.value(simId).number;
        QString sim2Number;
        if (sim2Id > 0)
            sim2Number = simById.value(sim2Id).number;

        html += "<tr><td>" + QString::number(num++) +
                "</td>"
                "<td>" +
                serial.toHtmlEscaped() +
                "</td>"
                "<td>" +
                imei1.toHtmlEscaped() +
                "</td>"
                "<td>" +
                simNumber.toHtmlEscaped() +
                "</td>"
                "<td>" +
                imei2.toHtmlEscaped() +
                "</td>"
                "<td>" +
                sim2Number.toHtmlEscaped() + "</td></tr>";
    }
    html += "</table>";

    html += "<div class='signature'>"
            "<div><p>Передал (Арендодатель):</p><p>________________ / ____________</p></div>"
            "<div><p>Принял (Арендатор):</p><p>________________ / ____________</p></div>"
            "</div>";
    html += PrintService::docFooter();

    // Печать или сохранение в PDF
    PrintService::printHtml(html, this);
}
