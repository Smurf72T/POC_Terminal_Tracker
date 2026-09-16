#include "caseincomeform.h"
#include "ui_caseincomeform.h"
#include "database/databasemanager.h"
#include "database/repositories/caserepository.h"
#include "ui/delegates/comboboxdelegate.h"
#include "services/postactionlogger.h"
#include <QDateTime>
#include <QDateEdit>
#include <QLineEdit>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QTableView>
#include <QTextEdit>

namespace {
enum ColCaseIncome { ColCaseType = 0, ColQty = 1 };
}

CaseIncomeForm::CaseIncomeForm(QWidget* parent) : DocumentDialog(parent), ui(new Ui::CaseIncomeForm)
{
    ui->setupUi(this);
    setWindowTitle("Документ: Поступление чехлов");
    resize(700, 500);

    ui->dateEdit->setDate(QDate::currentDate());

    rowsModel->setHorizontalHeaderLabels({"Тип чехла", "Кол-во"});
    ui->tableView->setModel(rowsModel);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
    ui->tableView->setColumnWidth(ColCaseType, 300);

    // Типы из существующих чехлов + свободный ввод нового типа.
    QList<QPair<int, QString>> caseTypes;
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    if (query.exec("SELECT DISTINCT casetype FROM tblcases ORDER BY casetype")) {
        int idx = 1;
        while (query.next())
            caseTypes.append(qMakePair(idx++, query.value(0).toString()));
    }
    ui->tableView->setItemDelegateForColumn(ColCaseType, new ComboBoxDelegate(caseTypes, this, true));
}

CaseIncomeForm::~CaseIncomeForm()
{
    delete ui;
}

QString CaseIncomeForm::docType() const
{
    return "case_income";
}

QLineEdit* CaseIncomeForm::headerNumberEdit() const
{
    return ui->lineEditNumber;
}

QDateEdit* CaseIncomeForm::headerDateEdit() const
{
    return ui->dateEdit;
}

QTextEdit* CaseIncomeForm::headerCommentEdit() const
{
    return ui->textEditComment;
}

QTableView* CaseIncomeForm::tableView() const
{
    return ui->tableView;
}

void CaseIncomeForm::on_btnAddRow_clicked()
{
    int row = rowsModel->rowCount();
    rowsModel->insertRow(row);

    QStandardItem* typeItem = new QStandardItem();
    typeItem->setData(0, Qt::UserRole);
    typeItem->setData("", Qt::DisplayRole);

    QStandardItem* qtyItem = new QStandardItem();
    qtyItem->setData(QString(), Qt::DisplayRole);

    rowsModel->setItem(row, ColCaseType, typeItem);
    rowsModel->setItem(row, ColQty, qtyItem);
}

void CaseIncomeForm::on_btnDeleteRow_clicked()
{
    int row = ui->tableView->currentIndex().row();
    if (row >= 0) {
        rowsModel->removeRow(row);
    }
}

void CaseIncomeForm::on_btnPost_clicked()
{
    executePost();
}

void CaseIncomeForm::on_btnClose_clicked()
{
    close();
}

void CaseIncomeForm::loadSpecificEditData(int docId)
{
    m_originalQuantities.clear();

    const QSqlDatabase& db = DatabaseManager::instance().getDatabase();
    CaseRepository docs(db);

    const models::DocumentHeader header = docs.loadIncomeHeader(docId);
    if (header.id == 0) {
        QMessageBox::critical(this, "Ошибка", "Не удалось загрузить документ поступления чехлов.");
        return;
    }

    ui->lineEditNumber->setText(header.docNumber);
    ui->lineEditNumber->setReadOnly(true);
    ui->dateEdit->setDate(header.date);
    ui->textEditComment->setText(header.comments);
    setWindowTitle(QString("Редактирование поступления чехлов ID %1").arg(docId));

    const auto rows = docs.loadIncomeRows(docId);
    for (const auto& row : rows) {
        m_originalQuantities[row.caseType] = row.qty;

        int r = rowsModel->rowCount();
        rowsModel->insertRow(r);

        QStandardItem* typeItem = new QStandardItem();
        typeItem->setData(0, Qt::UserRole);
        typeItem->setData(row.caseType, Qt::DisplayRole);

        QStandardItem* qtyItem = new QStandardItem();
        qtyItem->setData(QString::number(row.qty), Qt::DisplayRole);

        rowsModel->setItem(r, ColCaseType, typeItem);
        rowsModel->setItem(r, ColQty, qtyItem);
    }
}

void CaseIncomeForm::onPostSuccess(int docId)
{
    if (m_editMode) {
        PostActionLogger::log("UPDATE", "tblcaseincomedocs", docId);
        QMessageBox::information(this, "Успех", "Документ поступления чехлов успешно обновлён!");
    } else {
        PostActionLogger::log("POST", "tblcaseincomedocs", docId);
        QMessageBox::information(this, "Успех", "Документ поступления чехлов успешно проведён!");
    }
    PostActionLogger::notify();
    this->close();
}