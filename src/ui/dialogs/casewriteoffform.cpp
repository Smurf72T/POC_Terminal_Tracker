#include "casewriteoffform.h"
#include "ui_casewriteoffform.h"
#include "database/databasemanager.h"
#include "ui/delegates/checkboxdelegate.h"
#include "ui/delegates/readonlydelegate.h"
#include "services/postactionlogger.h"
#include <QDateTime>
#include <QDateEdit>
#include <QLineEdit>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QTableView>
#include <QTextEdit>
#include <QDebug>
#include "utils/logging.h"

namespace {
enum ColCaseWriteoff { ColCheck = 0, ColType = 1, ColTerminal = 2, ColReason = 3 };
}

CaseWriteoffForm::CaseWriteoffForm(QWidget* parent) : DocumentDialog(parent), ui(new Ui::CaseWriteoffForm)
{
    ui->setupUi(this);
    setWindowTitle("Документ: Списание чехлов");
    resize(900, 600);

    ui->dateEdit->setDate(QDate::currentDate());
    // Список чехлов показывается целиком с галочками — кнопка добавления строки не нужна.
    ui->btnAddRow->setVisible(false);

    ui->tableView->setItemDelegateForColumn(ColCheck, new CheckBoxDelegate(this));
    ui->tableView->setItemDelegateForColumn(ColType, new ReadOnlyDelegate(this));
    ui->tableView->setItemDelegateForColumn(ColTerminal, new ReadOnlyDelegate(this));

    reloadCasesToTable();
}

CaseWriteoffForm::~CaseWriteoffForm()
{
    delete ui;
}

QString CaseWriteoffForm::docType() const
{
    return "case_writeoff";
}

QLineEdit* CaseWriteoffForm::headerNumberEdit() const
{
    return ui->lineEditNumber;
}

QDateEdit* CaseWriteoffForm::headerDateEdit() const
{
    return ui->dateEdit;
}

QTextEdit* CaseWriteoffForm::headerCommentEdit() const
{
    return ui->textEditComment;
}

QTableView* CaseWriteoffForm::tableView() const
{
    return ui->tableView;
}

void CaseWriteoffForm::reloadCasesToTable()
{
    rowsModel->clear();
    rowsModel->setHorizontalHeaderLabels({"Списать", "Тип чехла", "Терминал", "Причина"});
    ui->tableView->setModel(rowsModel);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
    ui->tableView->setColumnWidth(ColCheck, 60);
    ui->tableView->setColumnWidth(ColType, 180);
    ui->tableView->setColumnWidth(ColTerminal, 220);

    m_originalCaseIds.clear();

    // Причины из документа (режим редактирования).
    QMap<int, QString> reasonsOfDoc;
    if (m_editMode) {
        QSqlQuery q(DatabaseManager::instance().getDatabase());
        q.prepare("SELECT caseid, COALESCE(reason, '') FROM tblcasewriteoffdetails "
                  "WHERE casewriteoffdocid = :id");
        q.bindValue(":id", m_editDocId);
        if (q.exec()) {
            while (q.next()) {
                const int cid = q.value(0).toInt();
                if (cid > 0) {
                    reasonsOfDoc.insert(cid, q.value(1).toString());
                    m_originalCaseIds.insert(cid);
                }
            }
        }
    }

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("SELECT c.caseid, c.casetype, COALESCE(t.serialnumber, '') "
                  "FROM tblcases c "
                  "LEFT JOIN tblterminals t ON c.terminalid = t.terminalid "
                  "WHERE c.status IN (0, 1) "
                  "ORDER BY c.casetype, c.caseid");
    if (!query.exec()) {
        qCWarning(logSQL) << "loadCaseWriteoffCandidates failed:" << query.lastError().text();
        return;
    }

    while (query.next()) {
        const int caseId = query.value(0).toInt();
        const bool checked = reasonsOfDoc.contains(caseId);

        int r = rowsModel->rowCount();
        rowsModel->insertRow(r);

        QStandardItem* checkItem = new QStandardItem();
        checkItem->setData(caseId, Qt::UserRole);
        checkItem->setData(checked, Qt::DisplayRole);

        QStandardItem* typeItem = new QStandardItem();
        typeItem->setData(query.value(1).toString(), Qt::DisplayRole);

        QStandardItem* terminalItem = new QStandardItem();
        terminalItem->setData(query.value(2).toString(), Qt::DisplayRole);

        QStandardItem* reasonItem = new QStandardItem();
        reasonItem->setData(reasonsOfDoc.value(caseId), Qt::DisplayRole);

        rowsModel->setItem(r, ColCheck, checkItem);
        rowsModel->setItem(r, ColType, typeItem);
        rowsModel->setItem(r, ColTerminal, terminalItem);
        rowsModel->setItem(r, ColReason, reasonItem);
    }
}

void CaseWriteoffForm::on_btnAddRow_clicked()
{
    // Список кандидатов формируется автоматически — кнопка скрыта.
}

void CaseWriteoffForm::on_btnDeleteRow_clicked()
{
    int row = ui->tableView->currentIndex().row();
    if (row >= 0) {
        rowsModel->removeRow(row);
    }
}

void CaseWriteoffForm::on_btnPost_clicked()
{
    executePost();
}

void CaseWriteoffForm::on_btnClose_clicked()
{
    close();
}

void CaseWriteoffForm::loadSpecificEditData(int docId)
{
    const QSqlDatabase& db = DatabaseManager::instance().getDatabase();
    QSqlQuery query(db);
    query.prepare("SELECT docnumber, docdate, comments FROM tblcasewriteoffdocs WHERE casewriteoffdocid = :id");
    query.bindValue(":id", docId);
    if (!query.exec() || !query.next()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось загрузить документ списания чехлов.");
        return;
    }

    ui->lineEditNumber->setText(query.value(0).toString());
    ui->lineEditNumber->setReadOnly(true);
    ui->dateEdit->setDate(query.value(1).toDateTime().date());
    ui->textEditComment->setText(query.value(2).toString());
    setWindowTitle(QString("Редактирование списания чехлов ID %1").arg(docId));

    reloadCasesToTable();
}

void CaseWriteoffForm::onPostSuccess(int docId)
{
    if (m_editMode) {
        PostActionLogger::log("UPDATE", "tblcasewriteoffdocs", docId);
        QMessageBox::information(this, "Успех", "Документ списания чехлов успешно обновлён!");
    } else {
        PostActionLogger::log("POST", "tblcasewriteoffdocs", docId);
        QMessageBox::information(this, "Успех", "Документ списания чехлов успешно проведён!");
    }
    PostActionLogger::notify();
    this->close();
}