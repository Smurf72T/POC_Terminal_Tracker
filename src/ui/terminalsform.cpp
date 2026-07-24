#include "terminalsform.h"
#include "ui_terminalsform.h"
#include "../database/databasemanager.h"
#include <QMessageBox>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRelationalDelegate>
#include <QTimer>
#include <QLineEdit>
#include <QDateTime>
#include <QDebug>

TerminalsForm::TerminalsForm(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TerminalsForm)
{
    ui->setupUi(this);
    setWindowTitle("Справочник терминалов");
    resize(1000, 600);

    model = new QSqlRelationalTableModel(this, DatabaseManager::instance().getDatabase());
    model->setTable("tblterminals");
    model->setEditStrategy(QSqlRelationalTableModel::OnFieldChange);

    // Настраиваем связи (Внешние ключи) для выпадающих списков
    // Индекс 2 - это modelid. Связываем с tblmodels.
    model->setRelation(2, QSqlRelation("tblmodels", "modelid", "modelname"));
    // Индекс 6 - это currentsimcardid. Связываем с tblsimcards.
    model->setRelation(6, QSqlRelation("tblsimcards", "simcardid", "simnumber"));

    if (!model->select()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось загрузить терминалы: " + model->lastError().text());
        return;
    }

    // Настройка заголовков (порядок полей в БД!)
    // 0: terminalid, 1: serialnumber, 2: modelid, 3: imei1, 4: imei2,
    // 5: status, 6: currentsimcardid, 7: purchasedate, 8: notes, 9: createdat
    model->setHeaderData(0, Qt::Horizontal, "ID");
    model->setHeaderData(1, Qt::Horizontal, "Серийный номер");
    model->setHeaderData(2, Qt::Horizontal, "Модель");
    model->setHeaderData(3, Qt::Horizontal, "IMEI 1");
    model->setHeaderData(4, Qt::Horizontal, "IMEI 2");
    model->setHeaderData(5, Qt::Horizontal, "Статус (0-свободен, 1-аренда)");
    model->setHeaderData(6, Qt::Horizontal, "SIM-карта");
    model->setHeaderData(7, Qt::Horizontal, "Дата покупки");
    model->setHeaderData(8, Qt::Horizontal, "Примечание");

    ui->tableView->setModel(model);

    // Скрываем лишнее
    ui->tableView->hideColumn(0); // ID
    ui->tableView->hideColumn(9); // CreatedAt

    // Устанавливаем делегат для красивых ComboBox'ов в связанных полях
    ui->tableView->setItemDelegate(new QSqlRelationalDelegate(ui->tableView));

    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
    ui->tableView->setAlternatingRowColors(true);

    // Растягиваем важные колонки
    ui->tableView->setColumnWidth(1, 150);
    ui->tableView->setColumnWidth(3, 150);
    ui->tableView->setColumnWidth(4, 150);
}

TerminalsForm::~TerminalsForm()
{
    delete ui;
}

void TerminalsForm::on_btnAdd_clicked()
{
    // Проверяем, есть ли модели (нельзя добавить терминал без модели)
    QSqlQuery checkQuery(DatabaseManager::instance().getDatabase());
    checkQuery.exec("SELECT modelid FROM tblmodels LIMIT 1");

    if (!checkQuery.next()) {
        QMessageBox::warning(this, "Внимание", "Сначала добавьте модели в справочнике моделей!");
        return;
    }
    int defaultModelId = checkQuery.value(0).toInt();

    // Добавляем строку через модель (корректно для OnFieldChange)
    int row = model->rowCount();
    if (!model->insertRow(row)) {
        QMessageBox::critical(this, "Ошибка", "Не удалось добавить строку: " + model->lastError().text());
        return;
    }

    QString tempSerial = QString("SN-%1").arg(QDateTime::currentMSecsSinceEpoch() % 100000);
    QString tempImei = QString("000000000000000");

    // Заполняем поля
    QModelIndex serialIndex = model->index(row, 1);
    model->setData(serialIndex, tempSerial);

    QModelIndex modelIndex = model->index(row, 2);
    model->setData(modelIndex, defaultModelId);

    QModelIndex imei1Index = model->index(row, 3);
    model->setData(imei1Index, tempImei);

    QModelIndex imei2Index = model->index(row, 4);
    model->setData(imei2Index, tempImei);

    // Статус по умолчанию = 0 (Свободен) — уже установлено в БД

    ui->tableView->selectRow(row);
    ui->tableView->setCurrentIndex(serialIndex);

    // Начинаем редактирование серийного номера
    QTimer::singleShot(100, [this, serialIndex]() {
        ui->tableView->edit(serialIndex);
        QTimer::singleShot(50, [this, serialIndex]() {
            QWidget* editor = ui->tableView->indexWidget(serialIndex);
            if (editor) {
                QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor);
                if (lineEdit) lineEdit->selectAll();
            }
        });
    });
}

void TerminalsForm::on_btnDelete_clicked()
{
    int row = ui->tableView->currentIndex().row();
    if (row >= 0) {
        int id = model->data(model->index(row, 0)).toInt();
        QString serial = model->data(model->index(row, 1)).toString();
        int status = model->data(model->index(row, 5)).toInt();

        if (status == 1) {
            QMessageBox::warning(this, "Ошибка удаления", "Нельзя удалить терминал, который находится в аренде!");
            return;
        }

        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "Удаление",
            QString("Удалить терминал %1?").arg(serial),
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            QSqlQuery query(DatabaseManager::instance().getDatabase());
            query.prepare("DELETE FROM tblterminals WHERE terminalid = :id");
            query.bindValue(":id", id);

            if (query.exec()) {
                model->select();
            } else {
                QMessageBox::warning(this, "Ошибка", "Не удалось удалить.\n" + query.lastError().text());
            }
        }
    } else {
        QMessageBox::information(this, "Внимание", "Выберите строку.");
    }
}

void TerminalsForm::on_btnClose_clicked()
{
    close();
}