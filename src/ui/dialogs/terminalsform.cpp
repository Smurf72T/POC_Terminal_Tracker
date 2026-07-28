#include "terminalsform.h"
#include "ui_terminalsform.h"
#include "database/databasemanager.h"
#include "utils/validator.h"
#include <QMessageBox>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>
#include <QInputDialog>
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

    model = new QSqlQueryModel(this);
    ui->tableView->setModel(model);

    ui->tableView->hideColumn(0);

    model->setHeaderData(0, Qt::Horizontal, "ID");
    model->setHeaderData(1, Qt::Horizontal, "Серийный номер");
    model->setHeaderData(2, Qt::Horizontal, "Модель");
    model->setHeaderData(3, Qt::Horizontal, "IMEI 1");
    model->setHeaderData(4, Qt::Horizontal, "IMEI 2");
    model->setHeaderData(5, Qt::Horizontal, "Статус");
    model->setHeaderData(6, Qt::Horizontal, "SIM-карта");
    model->setHeaderData(7, Qt::Horizontal, "Дата покупки");
    model->setHeaderData(8, Qt::Horizontal, "Примечание");

    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
    ui->tableView->setAlternatingRowColors(true);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    ui->tableView->setColumnWidth(1, 150);
    ui->tableView->setColumnWidth(3, 150);
    ui->tableView->setColumnWidth(4, 150);

    loadModel();

    searchTimer = new QTimer(this);
    searchTimer->setSingleShot(true);
    searchTimer->setInterval(300);
    connect(searchTimer, &QTimer::timeout, this, [this]() {
        loadModel(ui->lineEditSearch->text());
    });
    connect(ui->lineEditSearch, &QLineEdit::textChanged, this, [this]() {
        searchTimer->start();
    });
}

TerminalsForm::~TerminalsForm()
{
    delete ui;
}

void TerminalsForm::loadModel(const QString &filter)
{
    QString queryStr =
        "SELECT t.terminalid, t.serialnumber, "
        "COALESCE(m.modelname, 'Неизвестная') AS modelname, "
        "t.imei1, t.imei2, "
        "CASE WHEN t.status = 0 THEN 'Свободен' ELSE 'В аренде' END AS status, "
        "COALESCE(s.simnumber, 'SIM не назначена') AS simnumber, "
        "t.purchasedate, t.notes "
        "FROM tblterminals t "
        "LEFT JOIN tblmodels m ON t.modelid = m.modelid "
        "LEFT JOIN tblsimcards s ON t.currentsimcardid = s.simcardid";

    QSqlQuery query(DatabaseManager::instance().getDatabase());

    if (!filter.isEmpty()) {
        QString likeFilter = "%" + filter + "%";
        queryStr += " WHERE (t.serialnumber LIKE :f1 "
                    "OR t.imei1 LIKE :f2 "
                    "OR t.imei2 LIKE :f3 "
                    "OR m.modelname LIKE :f4)";
        queryStr += " ORDER BY t.serialnumber";
        query.prepare(queryStr);
        query.bindValue(":f1", likeFilter);
        query.bindValue(":f2", likeFilter);
        query.bindValue(":f3", likeFilter);
        query.bindValue(":f4", likeFilter);
    } else {
        queryStr += " ORDER BY t.serialnumber";
        query.prepare(queryStr);
    }

    if (query.exec()) {
        model->setQuery(std::move(query));
    }
}

void TerminalsForm::on_btnAdd_clicked()
{
    QSqlQuery checkQuery(DatabaseManager::instance().getDatabase());
    checkQuery.exec("SELECT modelid FROM tblmodels ORDER BY modelid LIMIT 1");
    if (!checkQuery.next()) {
        QMessageBox::warning(this, "Внимание", "Сначала добавьте модели в справочнике моделей!");
        return;
    }
    int defaultModelId = checkQuery.value(0).toInt();

    bool ok;
    QString serial = QInputDialog::getText(this, "Добавление терминала",
        "Серийный номер:", QLineEdit::Normal,
        QString("SN-%1").arg(QDateTime::currentMSecsSinceEpoch() % 100000), &ok);
    if (!ok || serial.trimmed().isEmpty()) return;

    if (!Validator::validateSerialNotEmpty(serial)) {
        QMessageBox::warning(this, "Ошибка", "Серийный номер должен содержать минимум 3 символа.");
        return;
    }

    QString imei = QInputDialog::getText(this, "Добавление терминала",
        "IMEI 1:", QLineEdit::Normal, "000000000000000", &ok);
    if (!ok) return;

    QString imei2 = QInputDialog::getText(this, "Добавление терминала",
        "IMEI 2:", QLineEdit::Normal, "000000000000000", &ok);
    if (!ok) return;

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("INSERT INTO tblterminals (serialnumber, modelid, imei1, imei2, status) "
                  "VALUES (:serial, :modelid, :imei1, :imei2, 0)");
    query.bindValue(":serial", serial.trimmed());
    query.bindValue(":modelid", defaultModelId);
    query.bindValue(":imei1", imei);
    query.bindValue(":imei2", imei2);

    if (query.exec()) {
        loadModel();
    } else {
        QMessageBox::critical(this, "Ошибка", "Не удалось добавить терминал:\n" + query.lastError().text());
    }
}

void TerminalsForm::on_btnDelete_clicked()
{
    int row = ui->tableView->currentIndex().row();
    if (row < 0) {
        QMessageBox::information(this, "Внимание", "Выберите строку.");
        return;
    }

    int id = model->data(model->index(row, 0)).toInt();
    QString serial = model->data(model->index(row, 1)).toString();
    QString status = model->data(model->index(row, 5)).toString();

    if (status == "В аренде") {
        QMessageBox::warning(this, "Ошибка удаления", "Нельзя удалить терминал, который находится в аренде!");
        return;
    }

    QSqlQuery checkRefQuery(DatabaseManager::instance().getDatabase());
    checkRefQuery.prepare(
        "SELECT "
        "(SELECT COUNT(*) FROM tblreceiptdetails WHERE terminalid = :id) + "
        "(SELECT COUNT(*) FROM tblrentaldetails WHERE terminalid = :id) + "
        "(SELECT COUNT(*) FROM tblreturndetails WHERE terminalid = :id)");
    checkRefQuery.bindValue(":id", id);

    if (checkRefQuery.exec() && checkRefQuery.next() && checkRefQuery.value(0).toInt() > 0) {
        QMessageBox::warning(this, "Ошибка удаления",
            "Невозможно удалить терминал: на него ссылаются документы.\n"
            "Терминал будет деактивирован вместо удаления.");
        QSqlQuery deactivateQuery(DatabaseManager::instance().getDatabase());
        deactivateQuery.prepare("DELETE FROM tblterminals WHERE terminalid = :id");
        deactivateQuery.bindValue(":id", id);
        if (deactivateQuery.exec()) {
            loadModel();
        } else {
            QMessageBox::warning(this, "Ошибка", "Не удалось деактивировать: " + deactivateQuery.lastError().text());
        }
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
            loadModel();
        } else {
            QMessageBox::warning(this, "Ошибка", "Не удалось удалить.\n" + query.lastError().text());
        }
    }
}

void TerminalsForm::on_btnClose_clicked()
{
    close();
}
