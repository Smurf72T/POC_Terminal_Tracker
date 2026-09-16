#include "casesform.h"
#include "ui_casesform.h"
#include "database/databasemanager.h"
#include "database/submiterrortablemodel.h"
#include <QCloseEvent>
#include <QDateTime>
#include <QDebug>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlField>
#include <QSqlQuery>
#include <QSqlRelationalDelegate>
#include <QTimer>

class CaseStatusDelegate : public ReadOnlyDelegate {
public:
    explicit CaseStatusDelegate(QObject* parent = nullptr) : ReadOnlyDelegate(parent) {}

    QString displayText(const QVariant& value, const QLocale&) const override
    {
        bool ok;
        int status = value.toInt(&ok);
        if (ok) {
            switch (status) {
                case 0:
                    return QString::fromUtf8("На складе");
                case 1:
                    return QString::fromUtf8("Установлен");
                case 2:
                    return QString::fromUtf8("Списан");
            }
        }
        return value.toString();
    }
};

CasesForm::CasesForm(QWidget* parent) : QDialog(parent), ui(new Ui::CasesForm)
{
    ui->setupUi(this);
    setWindowTitle("Справочник чехлов");
    resize(800, 500);

    model = new SubmitErrorRelationalTableModel(this, DatabaseManager::instance().getDatabase());
    model->setTable("tblcases");
    model->setEditStrategy(QSqlRelationalTableModel::OnFieldChange);
    connect(static_cast<SubmitErrorRelationalTableModel*>(model), &SubmitErrorRelationalTableModel::submitFailed, this,
            [this](const QString& error) {
                QMessageBox::warning(this, "Ошибка сохранения", "Не удалось сохранить изменение:\n" + error);
            });

    // Колонка 3 (terminalid) отображается как серийный номер терминала.
    // LeftJoin обязателен: у свободных чехлов terminalid = NULL, иначе inner-join
    // QSqlRelationalTableModel отфильтрует все строки со склада.
    model->setRelation(3, QSqlRelation("tblterminals", "terminalid", "serialnumber"));
    model->setJoinMode(QSqlRelationalTableModel::LeftJoin);

    if (!model->select()) {
        QMessageBox::critical(this, "Ошибка БД",
                              "Не удалось загрузить чехлы: " + model->lastError().text() +
                                  "\n\nПроверьте соединение с базой данных.");
        ui->tableView->setEnabled(false);
        ui->btnAdd->setEnabled(false);
        ui->btnDelete->setEnabled(false);
        ui->lineEditSearch->setEnabled(false);
        return;
    }

    // Порядок колонок в tblcases:
    // 0: caseid, 1: casetype, 2: status, 3: terminalid, 4: notes, [5: createdat]
    model->setHeaderData(0, Qt::Horizontal, "ID");
    model->setHeaderData(1, Qt::Horizontal, "Тип чехла");
    model->setHeaderData(2, Qt::Horizontal, "Статус");
    model->setHeaderData(3, Qt::Horizontal, "Терминал");
    model->setHeaderData(4, Qt::Horizontal, "Примечание");
    if (model->columnCount() > 5) {
        model->setHeaderData(5, Qt::Horizontal, "Дата создания");
    }

    ui->tableView->setModel(model);
    ui->tableView->hideColumn(0);
    if (model->columnCount() > 5) {
        ui->tableView->hideColumn(5);
    }

    ui->tableView->setItemDelegate(new QSqlRelationalDelegate(ui->tableView));
    ui->tableView->setItemDelegateForColumn(2, new CaseStatusDelegate(ui->tableView));

    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
    ui->tableView->setAlternatingRowColors(true);

    ui->tableView->setColumnWidth(1, 200);
    ui->tableView->setColumnWidth(2, 140);
    ui->tableView->setColumnWidth(3, 200);
    ui->tableView->setColumnWidth(4, 200);

    ui->tableView->installEventFilter(this);

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

            QString filter = QString("casetype LIKE %1 ESCAPE '\\'").arg(likeVal);
            model->setFilter(filter);
        }
        model->select();
    });
    connect(ui->lineEditSearch, &QLineEdit::textChanged, this, [this]() { searchTimer->start(); });
}

CasesForm::~CasesForm()
{
    delete ui;
}

void CasesForm::on_btnAdd_clicked()
{
    bool ok = false;
    QString caseType = QInputDialog::getText(this, "Добавить чехол", "Тип чехла:", QLineEdit::Normal, QString(), &ok);
    if (!ok || caseType.trimmed().isEmpty()) {
        return;
    }

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("INSERT INTO tblcases (casetype, status) VALUES (:type, 0) RETURNING caseid");
    query.bindValue(":type", caseType.trimmed());

    if (query.exec() && query.next()) {
        int newId = query.value(0).toInt();
        if (!model->select()) {
            QMessageBox::critical(this, "Ошибка БД",
                                  "Запись создана, но не удалось обновить таблицу: " + model->lastError().text() +
                                      "\n\nПопробуйте перезапустить форму.");
            ui->tableView->setEnabled(false);
            return;
        }

        for (int row = 0; row < model->rowCount(); row++) {
            if (model->data(model->index(row, 0)).toInt() == newId) {
                QModelIndex index = model->index(row, 1);
                ui->tableView->selectRow(row);
                ui->tableView->setCurrentIndex(index);
                QTimer::singleShot(100, [this, index]() { ui->tableView->edit(index); });
                break;
            }
        }
    } else {
        QMessageBox::critical(this, "Ошибка добавления", "Не удалось добавить чехол:\n" + query.lastError().text());
    }
}

void CasesForm::on_btnDelete_clicked()
{
    int row = ui->tableView->currentIndex().row();
    if (row < 0) {
        QMessageBox::information(this, "Внимание", "Выберите строку для удаления.");
        return;
    }

    int id = model->data(model->index(row, 0)).toInt();
    QString caseType = model->data(model->index(row, 1)).toString();
    int status = model->data(model->index(row, 2)).toInt();

    if (status != 0) {
        QMessageBox::warning(this, "Ошибка удаления",
                             "Нельзя удалить чехол со статусом «Установлен» или «Списан»!\n"
                             "Сначала освободите чехол.");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Удаление", QString("Удалить чехол (%1) №%2?").arg(caseType).arg(id), QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        QSqlQuery query(DatabaseManager::instance().getDatabase());
        query.prepare("DELETE FROM tblcases WHERE caseid = :id");
        query.bindValue(":id", id);

        if (query.exec()) {
            model->select();
            QMessageBox::information(this, "Успех", "Чехол удалён.");
        } else {
            QMessageBox::warning(this, "Ошибка удаления", "Не удалось удалить чехол:\n" + query.lastError().text());
        }
    }
}

void CasesForm::on_btnClose_clicked()
{
    close();
}

bool CasesForm::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == ui->tableView && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_F9) {
            int row = ui->tableView->currentIndex().row();
            if (row < 0)
                return true;

            QString caseType = model->data(model->index(row, 1)).toString();
            QString notes = model->data(model->index(row, 4)).toString();

            QSqlQuery query(DatabaseManager::instance().getDatabase());
            query.prepare("INSERT INTO tblcases (casetype, status, notes) "
                          "VALUES (:type, 0, :notes) RETURNING caseid");
            query.bindValue(":type", caseType);
            query.bindValue(":notes", notes);

            if (query.exec() && query.next()) {
                int newId = query.value(0).toInt();
                model->select();

                for (int r = 0; r < model->rowCount(); r++) {
                    if (model->data(model->index(r, 0)).toInt() == newId) {
                        QModelIndex idx = model->index(r, 1);
                        ui->tableView->selectRow(r);
                        ui->tableView->setCurrentIndex(idx);
                        QTimer::singleShot(100, [this, idx]() { ui->tableView->edit(idx); });
                        break;
                    }
                }
            }
            return true;
        }
    }
    return QDialog::eventFilter(obj, event);
}

void CasesForm::closeEvent(QCloseEvent* event)
{
    if (model->isDirty()) {
        model->submitAll();
    }
    event->accept();
}