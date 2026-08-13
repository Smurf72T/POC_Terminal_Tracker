#include "batchstatusform.h"
#include "ui_batchstatusform.h"
#include "database/databasemanager.h"
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>

BatchStatusForm::BatchStatusForm(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BatchStatusForm),
    model(new QSqlQueryModel(this))
{
    ui->setupUi(this);
    setWindowTitle("Массовое обновление статусов");
    resize(800, 500);

    loadStatuses();

    ui->tableView->setModel(model);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(QAbstractItemView::MultiSelection);
    ui->tableView->setAlternatingRowColors(true);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    loadTerminals(0);
}

BatchStatusForm::~BatchStatusForm()
{
    delete ui;
}

void BatchStatusForm::loadStatuses()
{
    ui->comboBoxCurrentStatus->clear();
    ui->comboBoxCurrentStatus->addItem("Свободен (0)", 0);
    ui->comboBoxCurrentStatus->addItem("В аренде (1)", 1);
    ui->comboBoxCurrentStatus->addItem("В ремонте (2)", 2);
    ui->comboBoxCurrentStatus->addItem("Списан (3)", 3);
    ui->comboBoxCurrentStatus->addItem("Утерян (4)", 4);

    // «В аренде» (1) выставляется только через документ аренды — в массовом
    // обновлении этот статус недоступен.
    ui->comboBoxNewStatus->clear();
    ui->comboBoxNewStatus->addItem("Свободен (0)", 0);
    ui->comboBoxNewStatus->addItem("В ремонте (2)", 2);
    ui->comboBoxNewStatus->addItem("Списан (3)", 3);
    ui->comboBoxNewStatus->addItem("Утерян (4)", 4);
    ui->comboBoxNewStatus->setCurrentIndex(0);
}

void BatchStatusForm::loadTerminals(int currentStatus)
{
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare(
        "SELECT t.terminalid AS \"ID\", "
        "t.serialnumber AS \"Серийный номер\", "
        "COALESCE(m.modelname, '—') AS \"Модель\", "
        "CASE t.status WHEN 0 THEN 'Свободен' WHEN 1 THEN 'В аренде' WHEN 2 THEN 'В ремонте' WHEN 3 THEN 'Списан' WHEN 4 THEN 'Утерян' ELSE 'Прочее' END AS \"Статус\" "
        "FROM tblterminals t "
        "LEFT JOIN tblmodels m ON t.modelid = m.modelid "
        "WHERE t.status = :status AND t.is_deactivated = FALSE "
        "ORDER BY t.serialnumber"
    );
    query.bindValue(":status", currentStatus);

    if (!query.exec()) {
        QMessageBox::critical(this, "Ошибка", query.lastError().text());
        return;
    }

    model->setQuery(std::move(query));
    ui->tableView->resizeColumnsToContents();
    ui->tableView->hideColumn(0);
}

void BatchStatusForm::on_comboBoxCurrentStatus_currentIndexChanged(int index)
{
    int status = ui->comboBoxCurrentStatus->itemData(index).toInt();
    loadTerminals(status);
}

void BatchStatusForm::on_btnApply_clicked()
{
    QModelIndexList selected = ui->tableView->selectionModel()->selectedRows(0);

    if (selected.isEmpty()) {
        QMessageBox::warning(this, "Внимание", "Выберите хотя бы один терминал!");
        return;
    }

    int newStatus = ui->comboBoxNewStatus->currentData().toInt();

    // Списание (3) и утеря (4) — административные действия, доступны только админу.
    if ((newStatus == 3 || newStatus == 4) && !DatabaseManager::instance().isCurrentUserAdmin()) {
        QMessageBox::warning(this, "Доступ запрещён",
            "Изменение статуса на «Списан» / «Утерян» доступно только администратору.");
        return;
    }

    static const char *const kStatusNames[] = {
        "Свободен", "В аренде", "В ремонте", "Списан", "Утерян"
    };
    QString statusText = (newStatus >= 0 && newStatus <= 4)
        ? QString::fromUtf8(kStatusNames[newStatus])
        : QString::fromUtf8("Статус %1").arg(newStatus);

    QMessageBox::StandardButton reply = QMessageBox::question(this, "Подтверждение",
        QString("Изменить статус %1 терминалов на «%2»?").arg(selected.size()).arg(statusText),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    if (!db.transaction()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось начать транзакцию");
        return;
    }

    int updated = 0;
    int expectedStatus = ui->comboBoxCurrentStatus->currentData().toInt();
    for (const QModelIndex &idx : selected) {
        int terminalId = model->data(model->index(idx.row(), 0)).toInt();

        QSqlQuery lockQuery(db);
        lockQuery.prepare("SELECT status FROM tblterminals WHERE terminalid = :id FOR UPDATE NOWAIT");
        lockQuery.bindValue(":id", terminalId);

        if (!lockQuery.exec() || !lockQuery.next()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка",
                QString("Терминал %1 занят другим пользователем. Повторите попытку.").arg(terminalId));
            return;
        }

        if (lockQuery.value(0).toInt() != expectedStatus) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка",
                QString("Статус терминала %1 изменился. Перезагрузите список и повторите попытку.")
                    .arg(terminalId));
            return;
        }

        QSqlQuery query(db);
        query.prepare("UPDATE tblterminals SET status = :status WHERE terminalid = :id");
        query.bindValue(":status", newStatus);
        query.bindValue(":id", terminalId);

        if (query.exec()) {
            updated++;
        } else {
            db.rollback();
            QMessageBox::critical(this, "Ошибка",
                QString("Ошибка обновления терминала %1: %2").arg(terminalId).arg(query.lastError().text()));
            return;
        }
    }

    if (!db.commit()) {
        db.rollback();
        QMessageBox::critical(this, "Ошибка", "Не удалось зафиксировать транзакцию");
        return;
    }

    DatabaseManager::instance().notifyDataChanged();
    QMessageBox::information(this, "Успех", QString("Обновлено терминалов: %1").arg(updated));
    loadTerminals(ui->comboBoxCurrentStatus->currentData().toInt());
}

void BatchStatusForm::on_btnSelectAll_clicked()
{
    ui->tableView->selectAll();
}

void BatchStatusForm::on_btnDeselectAll_clicked()
{
    ui->tableView->clearSelection();
}

void BatchStatusForm::on_btnClose_clicked()
{
    close();
}
