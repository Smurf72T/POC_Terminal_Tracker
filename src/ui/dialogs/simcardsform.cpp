#include "simcardsform.h"
#include "ui_simcardsform.h"
#include "delegates/readonlydelegate.h"
#include "database/databasemanager.h"
#include <QMessageBox>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>
#include <QLineEdit>
#include <QDateTime>
#include <QDebug>
#include <QCloseEvent>
#include <QShortcut>
#include <QShortcut>

SIMCardsForm::SIMCardsForm(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SIMCardsForm)
{
    ui->setupUi(this);
    setWindowTitle("Справочник SIM-карт");
    resize(700, 500);

    // Используем таблицу tblsimcards напрямую
    model = new QSqlTableModel(this, DatabaseManager::instance().getDatabase());
    model->setTable("tblsimcards");
    model->setEditStrategy(QSqlTableModel::OnFieldChange);

    if (!model->select()) {
        QMessageBox::critical(this, "Ошибка БД",
            "Не удалось загрузить SIM-карты: " + model->lastError().text() +
            "\n\nПроверьте соединение с базой данных.");
        ui->tableView->setEnabled(false);
        ui->btnAdd->setEnabled(false);
        ui->btnDelete->setEnabled(false);
        ui->lineEditSearch->setEnabled(false);
        return;
    }

    // Порядок колонок в tblsimcards:
    // 0: simcardid, 1: simnumber, 2: status, 3: notes, [4: createdat]
    model->setHeaderData(0, Qt::Horizontal, "ID");
    model->setHeaderData(1, Qt::Horizontal, "Номер SIM");
    model->setHeaderData(2, Qt::Horizontal, "Статус");
    model->setHeaderData(3, Qt::Horizontal, "Примечание");
    if (model->columnCount() > 4) {
        model->setHeaderData(4, Qt::Horizontal, "Дата создания");
    }

    ui->tableView->setModel(model);

    // Скрываем служебные колонки
    ui->tableView->hideColumn(0); // ID
    if (model->columnCount() > 4) {
        ui->tableView->hideColumn(4); // Дата создания
    }

    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
    ui->tableView->setAlternatingRowColors(true);

    // Растягиваем колонки
    ui->tableView->setColumnWidth(1, 250);
    ui->tableView->setColumnWidth(2, 150);
    if (model->columnCount() > 3) {
        ui->tableView->setColumnWidth(3, 200);
    }

    // Делаем колонку статуса только для чтения (отображаем число как текст)
    ui->tableView->setItemDelegateForColumn(2, new ReadOnlyDelegate(ui->tableView));

    // F9 для дублирования строки
    QShortcut *shortcutF9 = new QShortcut(QKeySequence(Qt::Key_F9), this);
    connect(shortcutF9, &QShortcut::activated, this, [this]() {
        int row = ui->tableView->currentIndex().row();
        if (row < 0) return;

        QString simNumber = model->data(model->index(row, 1)).toString();
        int statusCode = model->data(model->index(row, 2)).toInt();
        QString notes = model->data(model->index(row, 3)).toString();

        QSqlQuery query(DatabaseManager::instance().getDatabase());
        query.prepare("INSERT INTO tblsimcards (simnumber, status, notes) "
                      "VALUES (:num, :status, :notes) RETURNING simcardid");
        query.bindValue(":num", simNumber);
        query.bindValue(":status", 0);
        query.bindValue(":notes", notes);

        if (query.exec() && query.next()) {
            int newId = query.value(0).toInt();
            model->select();

            for (int r = 0; r < model->rowCount(); r++) {
                if (model->data(model->index(r, 0)).toInt() == newId) {
                    QModelIndex idx = model->index(r, 1);
                    ui->tableView->selectRow(r);
                    ui->tableView->setCurrentIndex(idx);
                    QTimer::singleShot(100, [this, idx]() {
                        ui->tableView->edit(idx);
                    });
                    break;
                }
            }
        }
    });

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
            QString filter = QString("simnumber LIKE '%%1%%' ESCAPE '\\'")
                                .arg(escaped);
            model->setFilter(filter);
        }
        model->select();
    });
    connect(ui->lineEditSearch, &QLineEdit::textChanged, this, [this]() {
        searchTimer->start();
    });
}

SIMCardsForm::~SIMCardsForm()
{
    delete ui;
}

void SIMCardsForm::on_btnAdd_clicked()
{
    // Генерируем уникальный временный номер SIM (макс. 19 символов)
    static int addCounter = 0;
    QString tempNumber = QString("TMP%1%2")
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(addCounter++, 3, 10, QChar('0'));

    // Вставляем НАПРЯМУЮ в таблицу tblsimcards (не в VIEW!)
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("INSERT INTO tblsimcards (simnumber, status) VALUES (:number, 0) RETURNING simcardid");
    query.bindValue(":number", tempNumber);

    if (query.exec() && query.next()) {
        int newId = query.value(0).toInt();

        // Обновляем модель из таблицы
        if (!model->select()) {
            QMessageBox::critical(this, "Ошибка БД",
                "Запись создана, но не удалось обновить таблицу: " + model->lastError().text() +
                "\n\nПопробуйте перезапустить форму.");
            ui->tableView->setEnabled(false);
            return;
        }

        // Находим новую строку
        for (int row = 0; row < model->rowCount(); row++) {
            if (model->data(model->index(row, 0)).toInt() == newId) {
                QModelIndex index = model->index(row, 1); // Колонка с номером SIM
                ui->tableView->selectRow(row);
                ui->tableView->setCurrentIndex(index);

                // Начинаем редактирование номера SIM
                QTimer::singleShot(100, [this, index]() {
                    ui->tableView->edit(index);
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
    } else {
        QMessageBox::critical(this, "Ошибка добавления",
            "Не удалось добавить SIM-карту:\n" + query.lastError().text());
    }
}

void SIMCardsForm::on_btnDelete_clicked()
{
    int row = ui->tableView->currentIndex().row();
    if (row >= 0) {
        int id = model->data(model->index(row, 0)).toInt();
        QString number = model->data(model->index(row, 1)).toString();
        int status = model->data(model->index(row, 2)).toInt();

        // Проверяем, не установлена ли SIM-карта в терминал
        if (status == 1) {
            QMessageBox::warning(this, "Ошибка удаления",
                "Нельзя удалить SIM-карту, которая установлена в терминал!\n"
                "Сначала извлеките SIM-карту из терминала.");
            return;
        }

        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "Удаление",
            QString("Удалить SIM-карту %1?").arg(number),
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            // Удаляем НАПРЯМУЮ из таблицы tblsimcards
            QSqlQuery query(DatabaseManager::instance().getDatabase());
            query.prepare("DELETE FROM tblsimcards WHERE simcardid = :id");
            query.bindValue(":id", id);

            if (query.exec()) {
                model->select(); // Обновляем таблицу
                QMessageBox::information(this, "Успех", "SIM-карта удалена.");
            } else {
                QMessageBox::warning(this, "Ошибка удаления",
                    "Не удалось удалить SIM-карту:\n" + query.lastError().text());
            }
        }
    } else {
        QMessageBox::information(this, "Внимание", "Выберите строку для удаления.");
    }
}

void SIMCardsForm::on_btnClose_clicked()
{
    close();
}

void SIMCardsForm::closeEvent(QCloseEvent *event)
{
    if (model->isDirty()) {
        model->submitAll();
    }
    event->accept();
}