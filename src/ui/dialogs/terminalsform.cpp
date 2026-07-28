#include "terminalsform.h"
#include "ui_terminalsform.h"
#include "../../database/databasemanager.h"
#include "utils/validator.h"
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

    // Подключаем валидацию при изменении данных модели
    connect(model, &QSqlRelationalTableModel::dataChanged,
            this, &TerminalsForm::on_model_dataChanged);

    // Debounce timer для поиска
    static QTimer* searchTimer = nullptr;
    if (!searchTimer) {
        searchTimer = new QTimer(this);
        searchTimer->setSingleShot(true);
        searchTimer->setInterval(300);
        connect(searchTimer, &QTimer::timeout, this, [this]() {
            QString searchText = ui->lineEditSearch->text();
            if (searchText.isEmpty()) {
                model->setFilter("");
            } else {
                QString filter = QString("serialnumber LIKE '%%1%%' OR imei1 LIKE '%%1%%' OR imei2 LIKE '%%1%%'")
                                    .arg(searchText.replace("'", "''"));
                model->setFilter(filter);
            }
            model->select();
        });
    }
    connect(ui->lineEditSearch, &QLineEdit::textChanged, this, [this]() {
        searchTimer->start();
    });
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

void TerminalsForm::on_model_dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_UNUSED(bottomRight);

    int row = topLeft.row();
    int column = topLeft.column();

    // Валидация IMEI 1 (колонка 3) и IMEI 2 (колонка 4)
    if (column == 3 || column == 4) {
        QString imei = model->data(topLeft).toString();
        if (!imei.isEmpty() && !Validator::validateIMEI(imei)) {
            QMessageBox::warning(this, "Ошибка валидации",
                QString("Строка %1, IMEI %2: должно быть ровно 15 цифр.")
                    .arg(row + 1).arg(column == 3 ? "1" : "2"));
        }
    }

    // Валидация серийного номера (колонка 1)
    if (column == 1) {
        QString serial = model->data(topLeft).toString();
        if (!Validator::validateSerialNotEmpty(serial)) {
            QMessageBox::warning(this, "Ошибка валидации",
                QString("Строка %1: серийный номер должен содержать минимум 3 символа.").arg(row + 1));
        } else if (Validator::checkUniqueSerial(serial, model->data(model->index(row, 0)).toInt())) {
            // Уникальность уже проверена функцией
            Q_UNUSED(0);
        } else {
            QMessageBox::warning(this, "Ошибка валидации",
                QString("Строка %1: терминал с таким серийным номером уже существует.").arg(row + 1));
        }
    }
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