#include "siminstallform.h"
#include "ui_siminstallform.h"
#include "database/databasemanager.h"
#include "database/repositories/documentrepository.h"
#include "database/repositories/simcardrepository.h"
#include "database/repositories/terminalrepository.h"
#include "database/repositories/siminstallrepository.h"
#include "ui/delegates/comboboxdelegate.h"
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QTime>
#include <QDebug>
#include "utils/logging.h"
#include "services/documentnumbergenerator.h"
#include "services/postactionlogger.h"
#include <QSet>

SimInstallForm::SimInstallForm(QWidget* parent) : DocumentDialog(parent), ui(new Ui::SimInstallForm)
{
    ui->setupUi(this);
    setWindowTitle("Документ: Установка SIM в терминал");
    resize(900, 600);

    ui->dateEdit->setDate(QDate::currentDate());

    rowsModel->setColumnCount(3);
    rowsModel->setHorizontalHeaderLabels({"Терминал", "SIM (IMEI 1)", "SIM (IMEI 2)"});
    ui->tableView->setModel(rowsModel);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);

    loadFreeTerminalsToDelegate();
    loadFreeSIMsToDelegate();

    connect(rowsModel, &QStandardItemModel::dataChanged, this, &SimInstallForm::onTableViewDataChanged);
}

SimInstallForm::~SimInstallForm()
{
    delete ui;
}

QString SimInstallForm::docType() const
{
    return "sim_install";
}

QLineEdit* SimInstallForm::headerNumberEdit() const
{
    return ui->lineEditNumber;
}

QDateEdit* SimInstallForm::headerDateEdit() const
{
    return ui->dateEdit;
}

QTextEdit* SimInstallForm::headerCommentEdit() const
{
    return ui->textEditComment;
}

QTableView* SimInstallForm::tableView() const
{
    return ui->tableView;
}

void SimInstallForm::loadFreeTerminalsToDelegate()
{
    QList<QPair<int, QString>> terminals;
    m_installedSim1.clear();
    m_installedSim2.clear();
    const auto free = TerminalRepository(DatabaseManager::instance().getDatabase()).loadFreeForSelection();
    for (const auto& t : free) {
        terminals.append(qMakePair(t.id, t.serialNumber));
        if (t.currentSimCardId > 0) {
            models::SimCard sim = SimCardRepository(DatabaseManager::instance().getDatabase()).loadById(t.currentSimCardId);
            if (sim.id > 0)
                m_installedSim1.insert(t.id, qMakePair(sim.id, sim.number));
        }
        if (t.currentSimCard2Id > 0) {
            models::SimCard sim2 = SimCardRepository(DatabaseManager::instance().getDatabase()).loadById(t.currentSimCard2Id);
            if (sim2.id > 0)
                m_installedSim2.insert(t.id, qMakePair(sim2.id, sim2.number));
        }
    }

    ui->tableView->setItemDelegateForColumn(0, new ComboBoxDelegate(terminals, this));
}

void SimInstallForm::loadFreeSIMsToDelegate()
{
    QList<QPair<int, QString>> sims;
    QSet<int> seen;
    const auto free = SimCardRepository(DatabaseManager::instance().getDatabase()).loadFreeForSelection();
    for (const auto& s : free) {
        sims.append(qMakePair(s.id, s.number));
        seen.insert(s.id);
    }

    // Добавляем SIM, уже установленные в свободные терминалы, чтобы их
    // можно было видеть/выбирать в редакторе.
    auto appendInstalled = [&](const QMap<int, QPair<int, QString>>& installed) {
        for (auto it = installed.constBegin(); it != installed.constEnd(); ++it) {
            if (it.value().first > 0 && !seen.contains(it.value().first)) {
                sims.append(qMakePair(it.value().first, it.value().second));
                seen.insert(it.value().first);
            }
        }
    };
    appendInstalled(m_installedSim1);
    appendInstalled(m_installedSim2);

    ui->tableView->setItemDelegateForColumn(1, new ComboBoxDelegate(sims, this, true));
    ui->tableView->setItemDelegateForColumn(2, new ComboBoxDelegate(sims, this, true));
}

void SimInstallForm::onTableViewDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
    Q_UNUSED(bottomRight);

    int row = topLeft.row();
    int column = topLeft.column();

    // При выборе терминала — подставляем уже установленные в него SIM
    if (column == 0) {
        int terminalId = rowsModel->data(rowsModel->index(row, 0), Qt::UserRole).toInt();
        autoFillSimForTerminal(row, terminalId);
    }
}

void SimInstallForm::autoFillSimForTerminal(int row, int terminalId)
{
    if (terminalId <= 0) {
        rowsModel->setData(rowsModel->index(row, 1), 0, Qt::UserRole);
        rowsModel->setData(rowsModel->index(row, 1), QString(), Qt::DisplayRole);
        rowsModel->setData(rowsModel->index(row, 2), 0, Qt::UserRole);
        rowsModel->setData(rowsModel->index(row, 2), QString(), Qt::DisplayRole);
        return;
    }

    if (m_installedSim1.contains(terminalId)) {
        const auto& sim = m_installedSim1.value(terminalId);
        rowsModel->setData(rowsModel->index(row, 1), sim.first, Qt::UserRole);
        rowsModel->setData(rowsModel->index(row, 1), sim.second, Qt::DisplayRole);
    } else {
        rowsModel->setData(rowsModel->index(row, 1), 0, Qt::UserRole);
        rowsModel->setData(rowsModel->index(row, 1), QString(), Qt::DisplayRole);
    }

    if (m_installedSim2.contains(terminalId)) {
        const auto& sim2 = m_installedSim2.value(terminalId);
        rowsModel->setData(rowsModel->index(row, 2), sim2.first, Qt::UserRole);
        rowsModel->setData(rowsModel->index(row, 2), sim2.second, Qt::DisplayRole);
    } else {
        rowsModel->setData(rowsModel->index(row, 2), 0, Qt::UserRole);
        rowsModel->setData(rowsModel->index(row, 2), QString(), Qt::DisplayRole);
    }
}

void SimInstallForm::on_btnAddRow_clicked()
{
    int row = rowsModel->rowCount();
    rowsModel->insertRow(row);

    QStandardItem* terminalItem = new QStandardItem();
    terminalItem->setData(0, Qt::UserRole);
    terminalItem->setData("", Qt::DisplayRole);

    QStandardItem* simItem = new QStandardItem();
    simItem->setData(0, Qt::UserRole);
    simItem->setData("", Qt::DisplayRole);

    QStandardItem* sim2Item = new QStandardItem();
    sim2Item->setData(0, Qt::UserRole);
    sim2Item->setData("", Qt::DisplayRole);

    rowsModel->setItem(row, 0, terminalItem);
    rowsModel->setItem(row, 1, simItem);
    rowsModel->setItem(row, 2, sim2Item);
}

void SimInstallForm::on_btnDeleteRow_clicked()
{
    int row = ui->tableView->currentIndex().row();
    if (row >= 0) {
        rowsModel->removeRow(row);
    }
}

void SimInstallForm::on_btnPost_clicked()
{
    executePost();
}

void SimInstallForm::on_btnClose_clicked()
{
    close();
}

void SimInstallForm::loadSpecificEditData(int docId)
{
    m_originalDetails.clear();

    const QSqlDatabase& db = DatabaseManager::instance().getDatabase();
    SimInstallRepository docs(db);

    const models::SimInstallDocument header = docs.loadHeader(docId);
    if (header.id == 0) {
        QMessageBox::critical(this, "Ошибка", "Не удалось загрузить документ установки SIM.");
        return;
    }

    ui->lineEditNumber->setText(header.docNumber);
    ui->lineEditNumber->setReadOnly(true);
    ui->dateEdit->setDate(header.date);
    ui->textEditComment->setText(header.comments);
    setWindowTitle(QString("Редактирование установки SIM ID %1").arg(docId));

    const auto rows = docs.loadDetails(docId);
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

        rowsModel->setItem(r, 0, terminalItem);
        rowsModel->setItem(r, 1, simItem);
        rowsModel->setItem(r, 2, sim2Item);
    }
}

void SimInstallForm::onPostSuccess(int docId)
{
    if (m_editMode) {
        PostActionLogger::log("UPDATE", "tblsiminstalldocs", docId);
        QMessageBox::information(this, "Успех", "Документ установки SIM успешно обновлён!");
    } else {
        PostActionLogger::log("POST", "tblsiminstalldocs", docId);
        QMessageBox::information(this, "Успех", "Документ установки SIM успешно проведён!");
    }
    PostActionLogger::notify();
    this->close();
}
