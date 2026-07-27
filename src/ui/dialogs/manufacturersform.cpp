#include "manufacturersform.h"
#include "ui_manufacturersform.h"
#include "../../database/databasemanager.h"
#include <QDebug>
#include <QMessageBox>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTimer>
#include <QLineEdit>      // <-- Добавлено для Clangd
#include <QDateTime>      // <-- Добавлено для QDateTime::currentMSecsSinceEpoch()

ManufacturersForm::ManufacturersForm(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ManufacturersForm)
{
    ui->setupUi(this);
    setWindowTitle("Справочник производителей");
    resize(500, 400);

    // Проверка подключения к БД
    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    if (!db.isOpen()) {
        QMessageBox::critical(this, "Ошибка", "База данных не подключена!");
        return;
    }

    // Инициализация модели данных
    model = new QSqlTableModel(this, db);
    model->setTable("tblmanufacturers");
    model->setEditStrategy(QSqlTableModel::OnFieldChange);
    
    // Выполняем select и проверяем результат
    if (!model->select()) {
        QMessageBox::critical(this, "Ошибка загрузки данных",
            "Не удалось загрузить данные:\n" + model->lastError().text());
        return;
    }
    
    // Проверяем количество записей
    int rowCount = model->rowCount();
    qDebug() << "Загружено записей из tblmanufacturers:" << rowCount;
    
    if (rowCount == 0) {
        qDebug() << "Таблица пуста или не найдена";
    }

    // Настройка сортировки
    model->setSort(0, Qt::AscendingOrder);

    // Настройка заголовков
    model->setHeaderData(0, Qt::Horizontal, "ID");
    model->setHeaderData(1, Qt::Horizontal, "Название производителя");

    // Привязка модели к представлению
    ui->tableView->setModel(model);
    ui->tableView->hideColumn(0);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
    ui->tableView->setAlternatingRowColors(true);
    
    qDebug() << "Таблица настроена, модель привязана";
}

ManufacturersForm::~ManufacturersForm()
{
    delete ui;
}

void ManufacturersForm::on_btnAdd_clicked()
{
    // Генерируем уникальное временное имя
    QString tempName = QString("Новый производитель %1")
        .arg(QDateTime::currentMSecsSinceEpoch() % 10000);

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("INSERT INTO tblmanufacturers (ManufacturerName) VALUES (:name) RETURNING ManufacturerID");
    query.bindValue(":name", tempName);

    if (query.exec()) {
        if (query.next()) {
            int newId = query.value(0).toInt();

            // Обновляем модель
            model->select();

            // Находим новую строку
            for (int row = 0; row < model->rowCount(); row++) {
                if (model->data(model->index(row, 0)).toInt() == newId) {
                    QModelIndex index = model->index(row, 1);
                    ui->tableView->selectRow(row);
                    ui->tableView->setCurrentIndex(index);

                    // Выделяем весь текст для быстрой замены
                    QTimer::singleShot(100, [this, index]() {
                        ui->tableView->edit(index);
                        // После начала редактирования выделяем весь текст
                        QTimer::singleShot(50, [this, index]() {
                            QWidget* editor = ui->tableView->indexWidget(index);
                            if (editor) {
                                QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor);
                                if (lineEdit) {
                                    lineEdit->selectAll();
                                }
                            }
                        });
                    });

                    return;
                }
            }
        }
    } else {
        QMessageBox::critical(this, "Ошибка добавления",
            "Не удалось добавить запись:\n" + query.lastError().text());
    }
}

void ManufacturersForm::on_btnDelete_clicked()
{
    int row = ui->tableView->currentIndex().row();
    if (row >= 0) {
        int id = model->data(model->index(row, 0)).toInt();
        QString name = model->data(model->index(row, 1)).toString();

        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "Подтверждение удаления",
            QString("Вы действительно хотите удалить производителя \"%1\"?").arg(name),
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            QSqlQuery query(DatabaseManager::instance().getDatabase());
            query.prepare("DELETE FROM tblmanufacturers WHERE ManufacturerID = :id");
            query.bindValue(":id", id);

            if (query.exec()) {
                model->select(); // Обновляем модель
                QMessageBox::information(this, "Успех", "Запись удалена.");
            } else {
                QMessageBox::warning(this, "Ошибка удаления",
                    "Не удалось удалить запись:\n" + query.lastError().text());
            }
        }
    } else {
        QMessageBox::information(this, "Внимание", "Выберите строку для удаления.");
    }
}

void ManufacturersForm::on_btnClose_clicked()
{
    close();
}