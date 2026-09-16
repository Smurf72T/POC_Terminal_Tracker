#include "caseinstallform.h"
#include "ui_caseinstallform.h"
#include "database/databasemanager.h"
#include "database/repositories/caserepository.h"
#include "database/repositories/terminalrepository.h"
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
#include <QSet>
#include <QDebug>
#include "utils/logging.h"

namespace {
enum ColCaseInstall { ColTerminal = 0, ColCase = 1 };
}

CaseInstallForm::CaseInstallForm(QWidget* parent) : DocumentDialog(parent), ui(new Ui::CaseInstallForm)
{
    ui->setupUi(this);
    setWindowTitle("Документ: Установка чехлов");
    resize(900, 600);

    ui->dateEdit->setDate(QDate::currentDate());

    rowsModel->setColumnCount(2);
    rowsModel->setHorizontalHeaderLabels({"Терминал", "Чехол"});
    ui->tableView->setModel(rowsModel);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);

    loadTerminalsToDelegate();
    loadCasesToDelegate();

    connect(rowsModel, &QStandardItemModel::dataChanged, this, &CaseInstallForm::onTableViewDataChanged);
}

CaseInstallForm::~CaseInstallForm()
{
    delete ui;
}

QString CaseInstallForm::docType() const
{
    return "case_install";
}

QLineEdit* CaseInstallForm::headerNumberEdit() const
{
    return ui->lineEditNumber;
}

QDateEdit* CaseInstallForm::headerDateEdit() const
{
    return ui->dateEdit;
}

QTextEdit* CaseInstallForm::headerCommentEdit() const
{
    return ui->textEditComment;
}

QTableView* CaseInstallForm::tableView() const
{
    return ui->tableView;
}

void CaseInstallForm::loadTerminalsToDelegate()
{
    ensureTerminalsInDelegate(QList<int>());
}

void CaseInstallForm::ensureTerminalsInDelegate(const QList<int>& terminalIds)
{
    QList<QPair<int, QString>> terminals;
    QSet<int> seen;
    const QSqlDatabase& db = DatabaseManager::instance().getDatabase();

    // Свободные и в аренде (status 0/1), не деактивированные.
    const auto available = TerminalRepository(db).loadForCaseInstall();
    for (const auto& t : available) {
        terminals.append(qMakePair(t.id, t.serialNumber));
        seen.insert(t.id);
        if (t.currentCaseId > 0) {
            const models::CaseItem c = CaseRepository(db).loadById(t.currentCaseId);
            if (c.id > 0) {
                m_installedCase.insert(t.id, qMakePair(c.id, c.caseType));
                qCDebug(logSQL) << "Installed case" << c.id << c.caseType << "on terminal" << t.serialNumber;
            }
        }
    }

    // Терминалы документа (режим редактирования), которых нет в списке выше
    // (например, стали списанными после проведения) — держим их доступными.
    QList<int> missing;
    for (int id : terminalIds) {
        if (!seen.contains(id))
            missing.append(id);
    }
    if (!missing.isEmpty()) {
        const auto extra = TerminalRepository(db).loadByIds(missing);
        for (const auto& t : extra) {
            terminals.append(qMakePair(t.id, t.serialNumber));
            if (t.currentCaseId > 0) {
                const models::CaseItem c = CaseRepository(db).loadById(t.currentCaseId);
                if (c.id > 0)
                    m_installedCase.insert(t.id, qMakePair(c.id, c.caseType));
            }
        }
    }

    ui->tableView->setItemDelegateForColumn(ColTerminal, new ComboBoxDelegate(terminals, this));
}

void CaseInstallForm::loadCasesToDelegate(const QSet<int>& includeCaseIds)
{
    QList<QPair<int, QString>> cases;
    QSet<int> seen;
    const QSqlDatabase& db = DatabaseManager::instance().getDatabase();
    const auto free = CaseRepository(db).loadFreeForSelection();
    for (const auto& c : free) {
        cases.append(qMakePair(c.id, QString("%1 [%2]").arg(c.caseType).arg(c.id)));
        seen.insert(c.id);
    }

    // Добавляем чехлы, уже установленные в терминалы.
    for (auto it = m_installedCase.constBegin(); it != m_installedCase.constEnd(); ++it) {
        if (it.value().first > 0 && !seen.contains(it.value().first)) {
            cases.append(qMakePair(it.value().first,
                                   QString("%1 [%2]").arg(it.value().second).arg(it.value().first)));
            seen.insert(it.value().first);
        }
    }

    // Чехлы из строк документа (режим редактирования).
    QList<int> missing;
    for (int id : includeCaseIds) {
        if (!seen.contains(id))
            missing.append(id);
    }
    if (!missing.isEmpty()) {
        const auto extra = CaseRepository(db).loadByIds(missing);
        for (const auto& c : extra) {
            cases.append(qMakePair(c.id, QString("%1 [%2]").arg(c.caseType).arg(c.id)));
            seen.insert(c.id);
        }
    }

    // Пустая ячейка = снятие установленного чехла.
    cases.prepend(qMakePair(0, tr("(снять)")));
    ui->tableView->setItemDelegateForColumn(ColCase, new ComboBoxDelegate(cases, this));
}

void CaseInstallForm::onTableViewDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
    Q_UNUSED(bottomRight);

    int row = topLeft.row();
    int column = topLeft.column();

    if (column == ColTerminal) {
        int terminalId = rowsModel->data(rowsModel->index(row, ColTerminal), Qt::UserRole).toInt();
        autoFillCaseForTerminal(row, terminalId);
    }
}

void CaseInstallForm::autoFillCaseForTerminal(int row, int terminalId)
{
    if (m_installedCase.contains(terminalId)) {
        const auto& c = m_installedCase.value(terminalId);
        rowsModel->setData(rowsModel->index(row, ColCase), c.first, Qt::UserRole);
        rowsModel->setData(rowsModel->index(row, ColCase), QString("%1 [%2]").arg(c.second).arg(c.first), Qt::DisplayRole);
    } else {
        rowsModel->setData(rowsModel->index(row, ColCase), 0, Qt::UserRole);
        rowsModel->setData(rowsModel->index(row, ColCase), QString(), Qt::DisplayRole);
    }
}

void CaseInstallForm::on_btnAddRow_clicked()
{
    int row = rowsModel->rowCount();
    rowsModel->insertRow(row);

    QStandardItem* terminalItem = new QStandardItem();
    terminalItem->setData(0, Qt::UserRole);
    terminalItem->setData("", Qt::DisplayRole);

    QStandardItem* caseItem = new QStandardItem();
    caseItem->setData(0, Qt::UserRole);
    caseItem->setData("", Qt::DisplayRole);

    rowsModel->setItem(row, ColTerminal, terminalItem);
    rowsModel->setItem(row, ColCase, caseItem);
}

void CaseInstallForm::on_btnDeleteRow_clicked()
{
    int row = ui->tableView->currentIndex().row();
    if (row >= 0) {
        rowsModel->removeRow(row);
    }
}

void CaseInstallForm::on_btnPost_clicked()
{
    executePost();
}

void CaseInstallForm::on_btnClose_clicked()
{
    close();
}

void CaseInstallForm::loadSpecificEditData(int docId)
{
    m_originalDetails.clear();

    const QSqlDatabase& db = DatabaseManager::instance().getDatabase();
    CaseRepository docs(db);

    const models::DocumentHeader header = docs.loadInstallHeader(docId);
    if (header.id == 0) {
        QMessageBox::critical(this, "Ошибка", "Не удалось загрузить документ установки чехлов.");
        return;
    }

    ui->lineEditNumber->setText(header.docNumber);
    ui->lineEditNumber->setReadOnly(true);
    ui->dateEdit->setDate(header.date);
    ui->textEditComment->setText(header.comments);
    setWindowTitle(QString("Редактирование установки чехлов ID %1").arg(docId));

    const auto rows = docs.loadInstallRows(docId);
    for (const auto& row : rows) {
        m_originalDetails.insert(row.terminalId, row.caseId);

        int r = rowsModel->rowCount();
        rowsModel->insertRow(r);

        QStandardItem* terminalItem = new QStandardItem();
        terminalItem->setData(row.terminalId, Qt::UserRole);
        terminalItem->setData(row.terminalSerial, Qt::DisplayRole);

        QStandardItem* caseItem = new QStandardItem();
        caseItem->setData(row.caseId, Qt::UserRole);
        caseItem->setData(row.caseId > 0 ? QString("%1 [%2]").arg(row.caseType).arg(row.caseId) : QString(),
                          Qt::DisplayRole);

        rowsModel->setItem(r, ColTerminal, terminalItem);
        rowsModel->setItem(r, ColCase, caseItem);
    }

    // Делегаты должны включать терминалы (в т.ч. в аренде) и чехлы документа
    // во избежание подмены значения редактором на первый пункт списка.
    QList<int> docTerminals;
    QSet<int> docCases;
    for (const auto& row : rows) {
        docTerminals.append(row.terminalId);
        if (row.caseId > 0)
            docCases.insert(row.caseId);
    }
    ensureTerminalsInDelegate(docTerminals);
    loadCasesToDelegate(docCases);
}

void CaseInstallForm::onPostSuccess(int docId)
{
    if (m_editMode) {
        PostActionLogger::log("UPDATE", "tblcaseinstalldocs", docId);
        QMessageBox::information(this, "Успех", "Документ установки чехлов успешно обновлён!");
    } else {
        PostActionLogger::log("POST", "tblcaseinstalldocs", docId);
        QMessageBox::information(this, "Успех", "Документ установки чехлов успешно проведён!");
    }
    PostActionLogger::notify();
    this->close();
}