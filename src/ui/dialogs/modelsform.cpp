#include "modelsform.h"
#include "ui_modelsform.h"
#include "database/databasemanager.h"
#include "database/submiterrortablemodel.h"
#include <QMessageBox>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRelationalDelegate>
#include <QTimer>
#include <QLineEdit>
#include <QDateTime>
#include <QDebug>
#include <QSqlField>
#include <QSqlDriver>
#include "terminalsform.h"

ModelsForm::ModelsForm(QWidget* parent) : QDialog(parent), ui(new Ui::ModelsForm)
{
    ui->setupUi(this);
    setWindowTitle("Справочник моделей");
    resize(600, 400);

    // Инициализация реляционной модели
    model = new SubmitErrorRelationalTableModel(this, DatabaseManager::instance().getDatabase());
    model->setTable("tblmodels");
    model->setEditStrategy(QSqlRelationalTableModel::OnFieldChange);
    connect(static_cast<SubmitErrorRelationalTableModel*>(model), &SubmitErrorRelationalTableModel::submitFailed, this,
            [this](const QString& error) {
                QMessageBox::warning(this, "Ошибка сохранения", "Не удалось сохранить изменение:\n" + error);
            });

    // ВАЖНО: Настраиваем связь (Внешний ключ)
    // Колонка 1 (manufacturerid) ссылается на tblmanufacturers.manufacturerid,
    // но отображать будем tblmanufacturers.manufacturername
    model->setRelation(1, QSqlRelation("tblmanufacturers", "manufacturerid", "manufacturername"));

    if (!model->select()) {
        QMessageBox::critical(this, "Ошибка БД",
                              "Не удалось загрузить модели: " + model->lastError().text() +
                                  "\n\nПроверьте соединение с базой данных.");
        ui->tableView->setEnabled(false);
        ui->btnAdd->setEnabled(false);
        ui->btnDelete->setEnabled(false);
        ui->lineEditSearch->setEnabled(false);
        return;
    }

    // Настройка заголовков
    model->setHeaderData(0, Qt::Horizontal, "ID");
    model->setHeaderData(1, Qt::Horizontal, "Производитель");
    model->setHeaderData(2, Qt::Horizontal, "Название модели");

    // Привязка к таблице
    ui->tableView->setModel(model);
    ui->tableView->hideColumn(0); // Скрываем ID

    // ВАЖНО: Устанавливаем делегат, чтобы в колонке "Производитель" был ComboBox
    ui->tableView->setItemDelegate(new QSqlRelationalDelegate(ui->tableView));

    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
    ui->tableView->setAlternatingRowColors(true);

    // Debounce timer для поиска
    searchTimer = new QTimer(this);
    searchTimer->setSingleShot(true);
    searchTimer->setInterval(300);
    connect(searchTimer, &QTimer::timeout, this, [this]() {
        QString searchText = ui->lineEditSearch->text();
        if (searchText.isEmpty()) {
            model->setFilter("");
        } else {
            QString escaped = searchText;
            escaped.replace("\\", "\\\\");
            escaped.replace("%", "\\%");
            escaped.replace("_", "\\_");

            QSqlField f("", QMetaType::fromType<QString>());
            f.setValue("%" + escaped + "%");
            QString likeVal = DatabaseManager::instance().getDatabase().driver()->formatValue(f);

            QString filter =
                QString("modelname LIKE %1 ESCAPE '\\' OR manufacturername LIKE %1 ESCAPE '\\'").arg(likeVal);
            model->setFilter(filter);
        }
        model->select();
    });
    connect(ui->lineEditSearch, &QLineEdit::textChanged, this, [this]() { searchTimer->start(); });
}

ModelsForm::~ModelsForm()
{
    delete ui;
}

void ModelsForm::on_btnAdd_clicked()
{
    // Сначала проверяем, есть ли вообще производители
    QSqlQuery checkQuery(DatabaseManager::instance().getDatabase());
    if (!checkQuery.exec("SELECT manufacturerid FROM tblmanufacturers ORDER BY manufacturerid LIMIT 1")) {
        QMessageBox::warning(this, "Ошибка БД",
                             "Не удалось проверить наличие производителей: " + checkQuery.lastError().text());
        return;
    }

    if (!checkQuery.next()) {
        QMessageBox::warning(this, "Внимание", "Сначала добавьте хотя бы одного производителя в справочнике!");
        return;
    }
    int defaultManId = checkQuery.value(0).toInt();

    // Генерируем временное имя
    QString tempName = QString("Новая модель %1").arg(QDateTime::currentMSecsSinceEpoch() % 10000);

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("INSERT INTO tblmodels (manufacturerid, modelname) VALUES (:manid, :name) RETURNING modelid");
    query.bindValue(":manid", defaultManId);
    query.bindValue(":name", tempName);

    if (query.exec() && query.next()) {
        int newId = query.value(0).toInt();

        model->select(); // Обновляем модель

        // Ищем новую строку
        for (int row = 0; row < model->rowCount(); row++) {
            if (model->data(model->index(row, 0)).toInt() == newId) {
                QModelIndex index = model->index(row, 2); // Колонка 2 - название модели
                ui->tableView->selectRow(row);
                ui->tableView->setCurrentIndex(index);

                QTimer::singleShot(100, [this, index]() {
                    ui->tableView->edit(index);
                    QTimer::singleShot(50, [this, index]() {
                        QWidget* editor = ui->tableView->indexWidget(index);
                        if (editor) {
                            QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor);
                            if (lineEdit)
                                lineEdit->selectAll();
                        }
                    });
                });
                return;
            }
        }
    } else {
        QMessageBox::critical(this, "Ошибка", "Не удалось добавить модель: " + query.lastError().text());
    }
}

void ModelsForm::on_btnDelete_clicked()
{
    int row = ui->tableView->currentIndex().row();
    if (row >= 0) {
        int id = model->data(model->index(row, 0)).toInt();
        QString name = model->data(model->index(row, 2)).toString();

        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "Удаление", QString("Удалить модель \"%1\"?").arg(name), QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            QSqlQuery query(DatabaseManager::instance().getDatabase());
            query.prepare("DELETE FROM tblmodels WHERE modelid = :id");
            query.bindValue(":id", id);

            if (query.exec()) {
                if (!model->select()) {
                    QMessageBox::critical(this, "Ошибка БД",
                                          "Запись удалена, но не удалось обновить таблицу: " +
                                              model->lastError().text() + "\n\nПопробуйте перезапустить форму.");
                    ui->tableView->setEnabled(false);
                }
            } else {
                QMessageBox::warning(this, "Ошибка",
                                     "Не удалось удалить. Возможно, есть привязанные терминалы.\n" +
                                         query.lastError().text());
            }
        }
    } else {
        QMessageBox::information(this, "Внимание", "Выберите строку.");
    }
}

void ModelsForm::on_btnClose_clicked()
{
    close();
}