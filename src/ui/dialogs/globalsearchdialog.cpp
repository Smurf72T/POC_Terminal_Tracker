#include "dialogs/globalsearchdialog.h"

#include "database/databasemanager.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSqlError>
#include <QSqlQuery>
#include <QVBoxLayout>

namespace {

// Позиция результата в QListWidget::itemData (Qt::UserRole=тип, +1=id).
struct SearchResult {
    int type; // 1=terminal, 2=client, 3=sim, 4=model, 5=manufacturer
    int id;
    QString text;
};

} // namespace

GlobalSearchDialog::GlobalSearchDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("Глобальный поиск (Ctrl+K)");
    resize(550, 400);
    setStyleSheet("QDialog { background-color: #252526; }");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    m_input = new QLineEdit(this);
    m_input->setPlaceholderText("Введите запрос (серийник, IMEI, клиент, SIM, модель...)");
    m_input->setClearButtonEnabled(true);
    layout->addWidget(m_input);

    m_list = new QListWidget(this);
    m_list->setAlternatingRowColors(true);
    layout->addWidget(m_list);

    auto* btnLayout = new QHBoxLayout();
    auto* btnOpen = new QPushButton("Открыть", this);
    auto* btnCancel = new QPushButton("Отмена", this);
    btnLayout->addStretch();
    btnLayout->addWidget(btnOpen);
    btnLayout->addWidget(btnCancel);
    layout->addLayout(btnLayout);

    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(btnOpen, &QPushButton::clicked, this, &GlobalSearchDialog::openCurrentItem);
    connect(m_input, &QLineEdit::textChanged, this, &GlobalSearchDialog::performSearch);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this]() { openCurrentItem(); });
}

int GlobalSearchDialog::selectedType() const
{
    auto* item = m_list->currentItem();
    return item ? item->data(Qt::UserRole).toInt() : 0;
}

int GlobalSearchDialog::selectedId() const
{
    auto* item = m_list->currentItem();
    return item ? item->data(Qt::UserRole + 1).toInt() : 0;
}

void GlobalSearchDialog::openCurrentItem()
{
    auto* item = m_list->currentItem();
    if (!item || item->data(Qt::UserRole).isNull())
        return;
    emit itemActivated(item->data(Qt::UserRole).toInt(), item->data(Qt::UserRole + 1).toInt());
    accept();
}

void GlobalSearchDialog::performSearch(const QString& text)
{
    m_list->clear();
    QString q = text.trimmed();
    if (q.length() < 2)
        return;

    m_list->addItem("Поиск...");

    QList<SearchResult> results;
    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    QString like = "%" + q.replace("\\", "\\\\").replace("'", "''").replace("%", "\\%").replace("_", "\\_") + "%";

    QSqlQuery query(db);
    query.prepare("SELECT terminalid, serialnumber, COALESCE(imei1,''), COALESCE(imei2,'') "
                  "FROM tblterminals WHERE serialnumber ILIKE :q "
                  "OR imei1 ILIKE :q2 OR imei2 ILIKE :q3 LIMIT 15");
    query.bindValue(":q", like);
    query.bindValue(":q2", like);
    query.bindValue(":q3", like);
    if (query.exec()) {
        while (query.next())
            results.append({1, query.value(0).toInt(), query.value(1).toString() + " (Терминал)"});
    }

    query.prepare("SELECT clientid, clientname FROM tblclients WHERE clientname ILIKE :q OR inn ILIKE :q2 LIMIT 10");
    query.bindValue(":q", like);
    query.bindValue(":q2", like);
    if (query.exec()) {
        while (query.next())
            results.append({2, query.value(0).toInt(), query.value(1).toString() + " (Клиент)"});
    }

    query.prepare("SELECT simcardid, simnumber FROM tblsimcards WHERE simnumber ILIKE :q LIMIT 10");
    query.bindValue(":q", like);
    if (query.exec()) {
        while (query.next())
            results.append({3, query.value(0).toInt(), query.value(1).toString() + " (SIM)"});
    }

    query.prepare("SELECT modelid, modelname FROM tblmodels WHERE modelname ILIKE :q LIMIT 10");
    query.bindValue(":q", like);
    if (query.exec()) {
        while (query.next())
            results.append({4, query.value(0).toInt(), query.value(1).toString() + " (Модель)"});
    }

    query.prepare(
        "SELECT manufacturerid, manufacturername FROM tblmanufacturers WHERE manufacturername ILIKE :q LIMIT 5");
    query.bindValue(":q", like);
    if (query.exec()) {
        while (query.next())
            results.append({5, query.value(0).toInt(), query.value(1).toString() + " (Производитель)"});
    }

    m_list->clear();
    for (const auto& r : results) {
        auto* item = new QListWidgetItem(r.text);
        item->setData(Qt::UserRole, r.type);
        item->setData(Qt::UserRole + 1, r.id);
        m_list->addItem(item);
    }
    if (results.isEmpty())
        m_list->addItem("Ничего не найдено");
}