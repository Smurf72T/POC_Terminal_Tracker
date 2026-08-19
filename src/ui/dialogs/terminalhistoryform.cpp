#include "terminalhistoryform.h"
#include "ui_terminalhistoryform.h"
#include "database/databasemanager.h"
#include "utils/reportexporter.h"
#include <QMessageBox>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QDateTime>
#include <QFileDialog>

TerminalHistoryForm::TerminalHistoryForm(int terminalId, QString serialNumber, QWidget* parent) :
    QDialog(parent), ui(new Ui::TerminalHistoryForm), receiptModel(nullptr), rentalModel(nullptr), returnModel(nullptr),
    paymentModel(nullptr)
{
    ui->setupUi(this);
    setWindowTitle("История терминала");
    resize(1100, 700);

    ui->lblTerminalInfo->setText(QString("Терминал: %1 (ID: %2)").arg(serialNumber).arg(terminalId));

    QSqlDatabase db = DatabaseManager::instance().getDatabase();

    QStringList loadErrors;

    receiptModel = new QSqlQueryModel(this);
    QSqlQuery receiptQuery(db);
    receiptQuery.prepare("SELECT rd.receiptdocid, rd.docnumber, rd.docdate, t.serialnumber, m.modelname "
                         "FROM tblreceiptdocs rd "
                         "JOIN tblreceiptdetails rdet ON rd.receiptdocid = rdet.receiptdocid "
                         "JOIN tblterminals t ON rdet.terminalid = t.terminalid "
                         "LEFT JOIN tblmodels m ON t.modelid = m.modelid "
                         "WHERE t.terminalid = :tid "
                         "ORDER BY rd.docdate DESC");
    receiptQuery.bindValue(":tid", terminalId);
    if (!receiptQuery.exec())
        loadErrors << "Приходы: " + receiptQuery.lastError().text();
    receiptModel->setQuery(std::move(receiptQuery));

    rentalModel = new QSqlQueryModel(this);
    QSqlQuery rentalQuery(db);
    rentalQuery.prepare("SELECT rdo.rentaldocid, rdo.docnumber, rdo.docdate, t.serialnumber, c.clientname, "
                        "s.simnumber, s2.simnumber "
                        "FROM tblrentaldocs rdo "
                        "JOIN tblrentaldetails rdet ON rdo.rentaldocid = rdet.rentaldocid "
                        "JOIN tblterminals t ON rdet.terminalid = t.terminalid "
                        "LEFT JOIN tblclients c ON rdo.clientid = c.clientid "
                        "LEFT JOIN tblsimcards s ON rdet.simcardid = s.simcardid "
                        "LEFT JOIN tblsimcards s2 ON rdet.simcardid2 = s2.simcardid "
                        "WHERE t.terminalid = :tid "
                        "ORDER BY rdo.docdate DESC");
    rentalQuery.bindValue(":tid", terminalId);
    if (!rentalQuery.exec())
        loadErrors << "Аренда: " + rentalQuery.lastError().text();
    rentalModel->setQuery(std::move(rentalQuery));

    returnModel = new QSqlQueryModel(this);
    QSqlQuery returnQuery(db);
    returnQuery.prepare("SELECT ret.returndocid, ret.docnumber, ret.docdate, t.serialnumber, c.clientname "
                        "FROM tblreturndocs ret "
                        "JOIN tblreturndetails rdet ON ret.returndocid = rdet.returndocid "
                        "JOIN tblterminals t ON rdet.terminalid = t.terminalid "
                        "LEFT JOIN tblclients c ON ret.clientid = c.clientid "
                        "WHERE t.terminalid = :tid "
                        "ORDER BY ret.docdate DESC");
    returnQuery.bindValue(":tid", terminalId);
    if (!returnQuery.exec())
        loadErrors << "Возвраты: " + returnQuery.lastError().text();
    returnModel->setQuery(std::move(returnQuery));

    paymentModel = new QSqlQueryModel(this);
    QSqlQuery paymentQuery(db);
    paymentQuery.prepare("SELECT p.paymentid, "
                         "('ОП-' || p.paymentid::text) AS docnumber, "
                         "p.paymentdate, c.clientname, rdo.docnumber AS rent_doc "
                         "FROM tblpayments p "
                         "JOIN tblpayment_rental_links prl ON p.paymentid = prl.paymentid "
                         "JOIN tblrentaldocs rdo ON prl.rentaldocid = rdo.rentaldocid "
                         "JOIN tblrentaldetails rdet ON rdo.rentaldocid = rdet.rentaldocid "
                         "JOIN tblclients c ON rdo.clientid = c.clientid "
                         "WHERE rdet.terminalid = :tid "
                         "ORDER BY p.paymentdate DESC");
    paymentQuery.bindValue(":tid", terminalId);
    if (!paymentQuery.exec())
        loadErrors << "Оплаты: " + paymentQuery.lastError().text();
    paymentModel->setQuery(std::move(paymentQuery));

    receiptModel->setHeaderData(0, Qt::Horizontal, "ID");
    receiptModel->setHeaderData(1, Qt::Horizontal, "Номер документа");
    receiptModel->setHeaderData(2, Qt::Horizontal, "Дата");
    receiptModel->setHeaderData(3, Qt::Horizontal, "Серийный номер");
    receiptModel->setHeaderData(4, Qt::Horizontal, "Модель");
    ui->tableViewReceipt->setModel(receiptModel);
    ui->tableViewReceipt->hideColumn(0);
    ui->tableViewReceipt->setColumnWidth(1, 180);
    ui->tableViewReceipt->setColumnWidth(2, 120);
    ui->tableViewReceipt->setColumnWidth(3, 150);
    ui->tableViewReceipt->setColumnWidth(4, 200);

    rentalModel->setHeaderData(0, Qt::Horizontal, "ID");
    rentalModel->setHeaderData(1, Qt::Horizontal, "Номер документа");
    rentalModel->setHeaderData(2, Qt::Horizontal, "Дата");
    rentalModel->setHeaderData(3, Qt::Horizontal, "Серийный номер");
    rentalModel->setHeaderData(4, Qt::Horizontal, "Клиент");
    rentalModel->setHeaderData(5, Qt::Horizontal, "SIM (IMEI 1)");
    rentalModel->setHeaderData(6, Qt::Horizontal, "SIM (IMEI 2)");
    ui->tableViewRental->setModel(rentalModel);
    ui->tableViewRental->hideColumn(0);
    ui->tableViewRental->setColumnWidth(1, 180);
    ui->tableViewRental->setColumnWidth(2, 120);
    ui->tableViewRental->setColumnWidth(3, 150);
    ui->tableViewRental->setColumnWidth(4, 250);
    ui->tableViewRental->setColumnWidth(5, 190);
    ui->tableViewRental->setColumnWidth(6, 190);

    returnModel->setHeaderData(0, Qt::Horizontal, "ID");
    returnModel->setHeaderData(1, Qt::Horizontal, "Номер документа");
    returnModel->setHeaderData(2, Qt::Horizontal, "Дата");
    returnModel->setHeaderData(3, Qt::Horizontal, "Серийный номер");
    returnModel->setHeaderData(4, Qt::Horizontal, "Клиент");
    ui->tableViewReturn->setModel(returnModel);
    ui->tableViewReturn->hideColumn(0);
    ui->tableViewReturn->setColumnWidth(1, 180);
    ui->tableViewReturn->setColumnWidth(2, 120);
    ui->tableViewReturn->setColumnWidth(3, 150);
    ui->tableViewReturn->setColumnWidth(4, 250);

    paymentModel->setHeaderData(0, Qt::Horizontal, "ID");
    paymentModel->setHeaderData(1, Qt::Horizontal, "Номер документа");
    paymentModel->setHeaderData(2, Qt::Horizontal, "Дата");
    paymentModel->setHeaderData(3, Qt::Horizontal, "Клиент");
    paymentModel->setHeaderData(4, Qt::Horizontal, "Номер арендного док.");
    ui->tableViewPayment->setModel(paymentModel);
    ui->tableViewPayment->hideColumn(0);
    ui->tableViewPayment->setColumnWidth(1, 180);
    ui->tableViewPayment->setColumnWidth(2, 120);
    ui->tableViewPayment->setColumnWidth(3, 250);
    ui->tableViewPayment->setColumnWidth(4, 200);

    int totalDocs =
        receiptModel->rowCount() + rentalModel->rowCount() + returnModel->rowCount() + paymentModel->rowCount();
    if (totalDocs == 0) {
        ui->lblTerminalInfo->setText(ui->lblTerminalInfo->text() + "\nИстория документов отсутствует");
        ui->lblTerminalInfo->setStyleSheet("color: gray;");
    }

    if (!loadErrors.isEmpty()) {
        QMessageBox::warning(this, "Ошибка загрузки истории",
                             "Не удалось загрузить часть данных:\n" + loadErrors.join("\n"));
    }
}

TerminalHistoryForm::~TerminalHistoryForm()
{
    delete ui;
}

void TerminalHistoryForm::on_btnExportExcel_clicked()
{
    int currentIndex = ui->tabWidget->currentIndex();
    QString tabTitle = ui->tabWidget->tabText(currentIndex);

    QSqlQueryModel* currentModel = nullptr;
    switch (currentIndex) {
        case 0:
            currentModel = receiptModel;
            break;
        case 1:
            currentModel = rentalModel;
            break;
        case 2:
            currentModel = returnModel;
            break;
        case 3:
            currentModel = paymentModel;
            break;
        default:
            return;
    }

    QString filePath = QFileDialog::getSaveFileName(
        this, QString("Экспорт истории терминала — %1").arg(tabTitle),
        QString("terminal_history_%1.xlsx").arg(QDateTime::currentDateTime().toString("yyyyMMdd")),
        "Excel (*.xlsx);;Все файлы (*)");

    if (filePath.isEmpty())
        return;

    if (ReportExporter::exportModelToExcel(currentModel, tabTitle, filePath, this)) {
        QMessageBox::information(this, "Успех", QString("Данные экспортированы в:\n%1").arg(filePath));
    }
}

void TerminalHistoryForm::on_btnClose_clicked()
{
    close();
}
