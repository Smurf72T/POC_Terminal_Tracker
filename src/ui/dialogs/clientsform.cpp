#include "clientsform.h"
#include "ui_clientsform.h"
#include "database/databasemanager.h"
#include <QMessageBox>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>
#include <QLineEdit>
#include <QDateTime>
#include <QDebug>

ClientsForm::ClientsForm(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ClientsForm)
{
    ui->setupUi(this);
    setWindowTitle("Справочник клиентов");
    resize(800, 500);

    model = new QSqlTableModel(this, DatabaseManager::instance().getDatabase());
    model->setTable("tblclients");
    model->setEditStrategy(QSqlTableModel::OnFieldChange);

    if (!model->select()) {
        QMessageBox::critical(this, "Ошибка БД",
            "Не удалось загрузить клиентов: " + model->lastError().text() +
            "\n\nПроверьте соединение с базой данных.");
        ui->tableView->setEnabled(false);
        ui->btnAdd->setEnabled(false);
        ui->btnDelete->setEnabled(false);
        ui->lineEditSearch->setEnabled(false);
        return;
    }

    // Настройка заголовков (все поля из ТЗ)
    model->setHeaderData(0, Qt::Horizontal, "ID");
    model->setHeaderData(1, Qt::Horizontal, "Наименование");
    model->setHeaderData(2, Qt::Horizontal, "ИНН");
    model->setHeaderData(3, Qt::Horizontal, "Адрес");
    model->setHeaderData(4, Qt::Horizontal, "Телефон");
    model->setHeaderData(5, Qt::Horizontal, "Email");

    ui->tableView->setModel(model);
    ui->tableView->hideColumn(0); // ID
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
    ui->tableView->setAlternatingRowColors(true);

    // Растягиваем колонки
    ui->tableView->setColumnWidth(1, 250);
    ui->tableView->setColumnWidth(2, 100);
    ui->tableView->setColumnWidth(3, 200);

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
            escaped.replace("'", "''");
            escaped.replace("%", "\\%");
            escaped.replace("_", "\\_");
            QString filter = QString("clientname LIKE '%%1%%' ESCAPE '\\' OR inn LIKE '%%1%%' ESCAPE '\\'")
                                .arg(escaped);
            model->setFilter(filter);
        }
        model->select();
    });
    connect(ui->lineEditSearch, &QLineEdit::textChanged, this, [this]() {
        searchTimer->start();
    });
}

ClientsForm::~ClientsForm()
{
    delete ui;
}

void ClientsForm::on_btnAdd_clicked()
{
    QString tempName = QString("Новый клиент %1").arg(QDateTime::currentMSecsSinceEpoch() % 10000);

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("INSERT INTO tblclients (clientname) VALUES (:name) RETURNING clientid");
    query.bindValue(":name", tempName);

    if (query.exec() && query.next()) {
        int newId = query.value(0).toInt();
        model->select();

        for (int row = 0; row < model->rowCount(); row++) {
            if (model->data(model->index(row, 0)).toInt() == newId) {
                QModelIndex index = model->index(row, 1); // Название клиента
                ui->tableView->selectRow(row);
                ui->tableView->setCurrentIndex(index);

                QTimer::singleShot(100, [this, index]() {
                    ui->tableView->edit(index);
                    QTimer::singleShot(50, [this, index]() {
                        QWidget* editor = ui->tableView->indexWidget(index);
                        if (editor) {
                            QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor);
                            if (lineEdit) lineEdit->selectAll();
                        }
                    });
                });
                return;
            }
        }
    } else {
        QMessageBox::critical(this, "Ошибка", "Не удалось добавить: " + query.lastError().text());
    }
}

void ClientsForm::on_btnDelete_clicked()
{
    int row = ui->tableView->currentIndex().row();
    if (row >= 0) {
        int id = model->data(model->index(row, 0)).toInt();
        QString name = model->data(model->index(row, 1)).toString();

        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "Удаление",
            QString("Удалить клиента \"%1\"?").arg(name),
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            QSqlQuery query(DatabaseManager::instance().getDatabase());
            query.prepare("DELETE FROM tblclients WHERE clientid = :id");
            query.bindValue(":id", id);

            if (query.exec()) {
                if (!model->select()) {
                    QMessageBox::critical(this, "Ошибка БД",
                        "Данные удалены, но не удалось обновить таблицу: " + model->lastError().text() +
                        "\n\nПопробуйте перезапустить форму.");
                    ui->tableView->setEnabled(false);
                }
            } else {
                QMessageBox::warning(this, "Ошибка", "Не удалось удалить. Возможно, есть арендованные терминалы.\n" + query.lastError().text());
            }
        }
    } else {
        QMessageBox::information(this, "Внимание", "Выберите строку.");
    }
}

void ClientsForm::on_lineEditSearch_textChanged(const QString &arg1)
{
    Q_UNUSED(arg1);
    // Логика поиска обрабатывается через debounce timer в конструкторе
}

void ClientsForm::on_btnClose_clicked()
{
    close();
}