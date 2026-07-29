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
#include <QKeyEvent>

SIMCardsForm::SIMCardsForm(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SIMCardsForm)
{
    ui->setupUi(this);
    setWindowTitle("Справочник SIM-карт");
    resize(700, 500);

    // ИСПОЛЬЗУЕМ VIEW вместо таблицы!
    model = new QSqlTableModel(this, DatabaseManager::instance().getDatabase());
    model->setTable("vsimcards");
    model->setEditStrategy(QSqlTableModel::OnManualSubmit);

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

    // Порядок колонок в VIEW:
    // 0: simcardid, 1: simnumber, 2: status_text, 3: status_code, 4: notes, 5: createdat
    model->setHeaderData(0, Qt::Horizontal, "ID");
    model->setHeaderData(1, Qt::Horizontal, "Номер SIM");
    model->setHeaderData(2, Qt::Horizontal, "Статус");
    model->setHeaderData(3, Qt::Horizontal, "Код статуса");
    model->setHeaderData(4, Qt::Horizontal, "Примечание");
    model->setHeaderData(5, Qt::Horizontal, "Дата создания");

    ui->tableView->setModel(model);

    // Скрываем служебные колонки
    ui->tableView->hideColumn(0); // ID
    ui->tableView->hideColumn(3); // Код статуса (число, не нужно пользователю)
    ui->tableView->hideColumn(5); // Дата создания

    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
    ui->tableView->setAlternatingRowColors(true);

    // Растягиваем колонки
    ui->tableView->setColumnWidth(1, 250);
    ui->tableView->setColumnWidth(2, 150);
    ui->tableView->setColumnWidth(4, 200);

    // ВАЖНО: Делаем колонку статуса (индекс 2) только для чтения
    ui->tableView->setItemDelegateForColumn(2, new ReadOnlyDelegate(ui->tableView));

    // F9 для дублирования строки
    ui->tableView->installEventFilter(this);

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
    // Генерируем временный номер SIM (18 цифр + буква)
    QString tempNumber = QString("999999999999999999A%1")
        .arg(QDateTime::currentMSecsSinceEpoch() % 1000);

    // Вставляем НАПРЯМУЮ в таблицу tblsimcards (не в VIEW!)
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("INSERT INTO tblsimcards (simnumber, status) VALUES (:number, 0) RETURNING simcardid");
    query.bindValue(":number", tempNumber);

    if (query.exec() && query.next()) {
        int newId = query.value(0).toInt();

        // Обновляем модель из VIEW
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
        QString status = model->data(model->index(row, 2)).toString();

        // Проверяем, не установлена ли SIM-карта в терминал
        if (status == "Установлена") {
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
                model->select(); // Обновляем VIEW
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
        if (!model->submitAll()) {
            QMessageBox::critical(this, "Ошибка сохранения",
                "Не удалось сохранить изменения:\n" + model->lastError().text());
            event->ignore();
            return;
        }
        qDebug() << "SIMCards: Все изменения сохранены";
    }
    model->revertAll();
    event->accept();
}

bool SIMCardsForm::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->tableView && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_F9) {
            int row = ui->tableView->currentIndex().row();
            if (row < 0) return true;

            // Копируем данные из текущей строки
            int statusCode = model->data(model->index(row, 3)).toInt();
            QString notes = model->data(model->index(row, 4)).toString();

            // Генерируем новый номер SIM
            QString tempNumber = QString("999999999999999999A%1")
                .arg(QDateTime::currentMSecsSinceEpoch() % 1000);

            QSqlQuery query(DatabaseManager::instance().getDatabase());
            query.prepare("INSERT INTO tblsimcards (simnumber, status, notes) "
                          "VALUES (:num, :status, :notes) RETURNING simcardid");
            query.bindValue(":num", tempNumber);
            query.bindValue(":status", statusCode);
            query.bindValue(":notes", notes);

            if (query.exec() && query.next()) {
                int newId = query.value(0).toInt();
                model->select();

                // Находим новую строку и выделяем её
                for (int r = 0; r < model->rowCount(); r++) {
                    if (model->data(model->index(r, 0)).toInt() == newId) {
                        ui->tableView->selectRow(r);
                        ui->tableView->setCurrentIndex(model->index(r, 1));
                        break;
                    }
                }
            }
            return true;
        }
    }
    return QDialog::eventFilter(obj, event);
}