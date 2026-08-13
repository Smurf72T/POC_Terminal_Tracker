#include "terminalsform.h"
#include "ui_terminalsform.h"
#include "database/databasemanager.h"
#include "utils/terminal_status.h"
#include "utils/validator.h"
#include <QMessageBox>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>
#include <QInputDialog>
#include <QLineEdit>
#include <QDateTime>
#include <QDebug>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>

TerminalsForm::TerminalsForm(QWidget* parent) : QDialog(parent), ui(new Ui::TerminalsForm)
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
    model->setHeaderData(9, Qt::Horizontal, "Был в ремонте");
    model->setHeaderData(10, Qt::Horizontal, "Деактивирован");

    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
    ui->tableView->setAlternatingRowColors(true);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    ui->tableView->setColumnWidth(1, 150);
    ui->tableView->setColumnWidth(3, 150);
    ui->tableView->setColumnWidth(4, 150);

    // Пагинация: таблица может содержать сотни тысяч строк, поэтому
    // данные грузятся страницами (LIMIT/OFFSET), а не целиком в память.
    auto* pageBar = new QHBoxLayout;
    m_btnFirst = new QPushButton("Первая", this);
    m_btnPrev = new QPushButton("← Назад", this);
    m_pageLabel = new QLabel(this);
    m_pageLabel->setAlignment(Qt::AlignCenter);
    m_pageLabel->setMinimumWidth(220);
    m_btnNext = new QPushButton("Вперёд →", this);
    m_btnLast = new QPushButton("Последняя", this);
    pageBar->addWidget(m_btnFirst);
    pageBar->addWidget(m_btnPrev);
    pageBar->addStretch();
    pageBar->addWidget(m_pageLabel);
    pageBar->addStretch();
    pageBar->addWidget(m_btnNext);
    pageBar->addWidget(m_btnLast);
    ui->verticalLayout->insertLayout(2, pageBar);

    connect(m_btnFirst, &QPushButton::clicked, this, [this]() { goToPage(0); });
    connect(m_btnPrev, &QPushButton::clicked, this, [this]() { goToPage(m_offset - m_pageSize); });
    connect(m_btnNext, &QPushButton::clicked, this, [this]() { goToPage(m_offset + m_pageSize); });
    connect(m_btnLast, &QPushButton::clicked, this,
            [this]() { goToPage(qMax(0, ((m_totalRows - 1) / m_pageSize) * m_pageSize)); });

    loadModel();

    searchTimer = new QTimer(this);
    searchTimer->setSingleShot(true);
    searchTimer->setInterval(300);
    connect(searchTimer, &QTimer::timeout, this, [this]() {
        m_offset = 0;
        loadModel(ui->lineEditSearch->text());
    });
    connect(ui->lineEditSearch, &QLineEdit::textChanged, this, [this]() { searchTimer->start(); });
}

TerminalsForm::~TerminalsForm()
{
    delete ui;
}

void TerminalsForm::loadModel(const QString& filter)
{
    QString whereClause;
    QStringList likeBinds;
    if (!filter.isEmpty()) {
        QString escaped = filter;
        escaped.replace("\\", "\\\\");
        escaped.replace("%", "\\%");
        escaped.replace("_", "\\_");
        QString likeFilter = "%" + escaped + "%";
        whereClause = " WHERE (t.serialnumber LIKE :f1 "
                      "OR t.imei1 LIKE :f2 "
                      "OR t.imei2 LIKE :f3 "
                      "OR m.modelname LIKE :f4)";
        likeBinds = {likeFilter, likeFilter, likeFilter, likeFilter};
    }

    QSqlDatabase db = DatabaseManager::instance().getDatabase();

    // Общее число строк — для пагинации и счётчика «X–Y из Z».
    QSqlQuery countQuery(db);
    countQuery.prepare("SELECT COUNT(*) FROM tblterminals t "
                       "LEFT JOIN tblmodels m ON t.modelid = m.modelid" +
                       whereClause);
    for (int i = 0; i < likeBinds.size(); ++i)
        countQuery.bindValue(QString(":f%1").arg(i + 1), likeBinds.at(i));
    m_totalRows = 0;
    if (countQuery.exec() && countQuery.next())
        m_totalRows = countQuery.value(0).toInt();

    QString queryStr = "SELECT t.terminalid, t.serialnumber, "
                       "COALESCE(m.modelname, 'Неизвестная') AS modelname, "
                       "t.imei1, t.imei2, " +
                       TerminalStatus::sqlCaseExpression("t.status") +
                       " AS status, "
                       "COALESCE(s.simnumber, 'SIM не назначена') AS simnumber, "
                       "t.purchasedate, t.notes, "
                       "CASE WHEN t.was_repaired THEN 'Да' ELSE 'Нет' END AS was_repaired, "
                       "CASE WHEN t.is_deactivated THEN 'Да' ELSE 'Нет' END AS is_deactivated "
                       "FROM tblterminals t "
                       "LEFT JOIN tblmodels m ON t.modelid = m.modelid "
                       "LEFT JOIN tblsimcards s ON t.currentsimcardid = s.simcardid" +
                       whereClause + " ORDER BY t.serialnumber LIMIT :limit OFFSET :offset";

    QSqlQuery query(db);
    query.prepare(queryStr);
    for (int i = 0; i < likeBinds.size(); ++i)
        query.bindValue(QString(":f%1").arg(i + 1), likeBinds.at(i));
    query.bindValue(":limit", m_pageSize);
    query.bindValue(":offset", m_offset);

    if (query.exec()) {
        model->setQuery(std::move(query));
    }

    refreshPagination();
}

void TerminalsForm::refreshPagination()
{
    if (!m_pageLabel)
        return;

    int rows = model->rowCount();
    if (m_totalRows == 0) {
        m_pageLabel->setText("Нет записей");
        m_btnFirst->setEnabled(false);
        m_btnPrev->setEnabled(false);
        m_btnNext->setEnabled(false);
        m_btnLast->setEnabled(false);
        return;
    }

    int first = m_offset + 1;
    int last = m_offset + rows;
    m_pageLabel->setText(QString("Записи %1 – %2 из %3").arg(first).arg(last).arg(m_totalRows));

    bool hasPrev = m_offset > 0;
    bool hasNext = last < m_totalRows;
    m_btnFirst->setEnabled(hasPrev);
    m_btnPrev->setEnabled(hasPrev);
    m_btnNext->setEnabled(hasNext);
    m_btnLast->setEnabled(hasNext);
}

void TerminalsForm::goToPage(int newOffset)
{
    if (newOffset < 0)
        newOffset = 0;
    int maxOffset = qMax(0, ((m_totalRows - 1) / m_pageSize) * m_pageSize);
    if (newOffset > maxOffset)
        newOffset = maxOffset;
    if (newOffset == m_offset)
        return;

    m_offset = newOffset;
    loadModel(ui->lineEditSearch->text());
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
    QString serial = QInputDialog::getText(this, "Добавление терминала", "Серийный номер:", QLineEdit::Normal,
                                           QString("SN-%1").arg(QDateTime::currentMSecsSinceEpoch() % 100000), &ok);
    if (!ok || serial.trimmed().isEmpty())
        return;

    if (!Validator::validateSerialNotEmpty(serial)) {
        QMessageBox::warning(this, "Ошибка", "Серийный номер должен содержать минимум 3 символа.");
        return;
    }

    QString imei =
        QInputDialog::getText(this, "Добавление терминала", "IMEI 1:", QLineEdit::Normal, "000000000000000", &ok);
    if (!ok)
        return;

    QString imei2 =
        QInputDialog::getText(this, "Добавление терминала", "IMEI 2:", QLineEdit::Normal, "000000000000000", &ok);
    if (!ok)
        return;

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
    checkRefQuery.prepare("SELECT "
                          "(SELECT COUNT(*) FROM tblreceiptdetails WHERE terminalid = :id) + "
                          "(SELECT COUNT(*) FROM tblrentaldetails WHERE terminalid = :id) + "
                          "(SELECT COUNT(*) FROM tblreturndetails WHERE terminalid = :id)");
    checkRefQuery.bindValue(":id", id);

    if (checkRefQuery.exec() && checkRefQuery.next() && checkRefQuery.value(0).toInt() > 0) {
        QMessageBox::warning(this, "Деактивация",
                             "На терминал ссылаются документы, поэтому он не может быть удалён.\n"
                             "Терминал будет помечен как деактивированный (скрыт из выбора в новых документах).");
        QSqlQuery deactivateQuery(DatabaseManager::instance().getDatabase());
        deactivateQuery.prepare("UPDATE tblterminals SET is_deactivated = TRUE WHERE terminalid = :id");
        deactivateQuery.bindValue(":id", id);
        if (deactivateQuery.exec()) {
            DatabaseManager::instance().logAction("UPDATE", "tblterminals", id);
            DatabaseManager::instance().notifyDataChanged();
            loadModel();
        } else {
            QMessageBox::warning(this, "Ошибка", "Не удалось деактивировать: " + deactivateQuery.lastError().text());
        }
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Удаление", QString("Удалить терминал %1?").arg(serial), QMessageBox::Yes | QMessageBox::No);

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
