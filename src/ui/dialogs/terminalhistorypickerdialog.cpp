#include "dialogs/terminalhistorypickerdialog.h"

#include "database/databasemanager.h"
#include "utils/logging.h"

#include <QComboBox>
#include <QCompleter>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlError>
#include <QSqlQuery>
#include <QVBoxLayout>

TerminalHistoryPickerDialog::TerminalHistoryPickerDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("История терминала");
    resize(400, 100);

    auto* layout = new QVBoxLayout(this);
    auto* label = new QLabel("Введите или выберите серийный номер:", this);
    auto* combo = new QComboBox(this);
    combo->setEditable(true);
    combo->setInsertPolicy(QComboBox::NoInsert);

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    if (!query.exec("SELECT terminalid, serialnumber FROM tblterminals ORDER BY serialnumber")) {
        qCWarning(logSQL) << "Failed to load terminals:" << query.lastError().text();
        return;
    }

    while (query.next())
        combo->addItem(query.value(1).toString(), query.value(0));

    auto* completer = new QCompleter(combo->model(), this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    combo->setCompleter(completer);

    auto* btnLayout = new QHBoxLayout();
    auto* btnOk = new QPushButton("Открыть", this);
    auto* btnCancel = new QPushButton("Отмена", this);
    btnLayout->addStretch();
    btnLayout->addWidget(btnOk);
    btnLayout->addWidget(btnCancel);
    layout->addWidget(label);
    layout->addWidget(combo);
    layout->addLayout(btnLayout);

    connect(btnOk, &QPushButton::clicked, this, [this, combo]() {
        QString serial = combo->currentText().trimmed();
        if (serial.isEmpty())
            return;
        QSqlQuery findQuery(DatabaseManager::instance().getDatabase());
        findQuery.prepare("SELECT terminalid FROM tblterminals WHERE serialnumber = :sn");
        findQuery.bindValue(":sn", serial);
        if (findQuery.exec() && findQuery.next()) {
            m_terminalId = findQuery.value(0).toInt();
            m_serial = serial;
            accept();
        } else {
            QMessageBox::warning(this, "Ошибка", QString("Терминал с серийным номером «%1» не найден.").arg(serial));
        }
    });
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

int TerminalHistoryPickerDialog::terminalId() const
{
    return m_terminalId;
}

QString TerminalHistoryPickerDialog::serialNumber() const
{
    return m_serial;
}