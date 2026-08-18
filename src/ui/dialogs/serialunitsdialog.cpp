#include "serialunitsdialog.h"
#include "ui_serialunitsdialog.h"

#include <QHeaderView>
#include <QMessageBox>
#include <QSet>
#include <QTableWidgetItem>

SerialUnitsDialog::SerialUnitsDialog(int expectedCount, const QStringList& serials, const QStringList& imei1,
                                     const QStringList& imei2, const QString& title, QWidget* parent)
    : QDialog(parent), ui(new Ui::SerialUnitsDialog), m_expectedCount(expectedCount)
{
    ui->setupUi(this);
    setWindowTitle(title);
    resize(640, 420);

    ui->labelInfo->setText(
        QString("Каждая строка — один комплект: серийный номер и его IMEI 1 / IMEI 2. Нужно комплектов: %1.")
            .arg(m_expectedCount));

    ui->tableWidget->setColumnCount(3);
    ui->tableWidget->setHorizontalHeaderLabels({"Серийный номер", "IMEI 1", "IMEI 2"});
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableWidget->setEditTriggers(QAbstractItemView::AnyKeyPressed | QAbstractItemView::DoubleClicked);
    ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectItems);

    m_populating = true;
    const int rows = qMax(serials.size(), m_expectedCount);
    ui->tableWidget->setRowCount(rows);
    for (int r = 0; r < rows; ++r) {
        const QString s = r < serials.size() ? serials.at(r) : QString();
        const QString i1 = r < imei1.size() ? imei1.at(r) : QString();
        const QString i2 = r < imei2.size() ? imei2.at(r) : QString();
        for (int c = 0; c < 3; ++c)
            ui->tableWidget->setItem(r, c, new QTableWidgetItem(c == 0 ? s : (c == 1 ? i1 : i2)));
    }
    m_populating = false;

    connect(ui->tableWidget, &QTableWidget::cellChanged, this, &SerialUnitsDialog::onCellChanged);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &SerialUnitsDialog::onAccepted);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &SerialUnitsDialog::reject);

    updateStatus();
}

SerialUnitsDialog::~SerialUnitsDialog()
{
    delete ui;
}

QStringList SerialUnitsDialog::serials() const
{
    QStringList result;
    for (int r = 0; r < ui->tableWidget->rowCount(); ++r) {
        QTableWidgetItem* it = ui->tableWidget->item(r, 0);
        const QString s = it ? it->text().trimmed() : QString();
        result.append(s.isEmpty() ? QString() : s);
    }
    return result;
}

QStringList SerialUnitsDialog::imei1() const
{
    QStringList result;
    for (int r = 0; r < ui->tableWidget->rowCount(); ++r) {
        QTableWidgetItem* it = ui->tableWidget->item(r, 1);
        const QString s = it ? it->text().trimmed() : QString();
        result.append(s.isEmpty() ? QString() : s);
    }
    return result;
}

QStringList SerialUnitsDialog::imei2() const
{
    QStringList result;
    for (int r = 0; r < ui->tableWidget->rowCount(); ++r) {
        QTableWidgetItem* it = ui->tableWidget->item(r, 2);
        const QString s = it ? it->text().trimmed() : QString();
        result.append(s.isEmpty() ? QString() : s);
    }
    return result;
}

void SerialUnitsDialog::updateStatus()
{
    const QStringList s = serials();
    const QStringList i1 = imei1();
    const QStringList i2 = imei2();

    int filled = 0;
    for (const QString& v : s) {
        if (!v.isEmpty())
            ++filled;
    }

    QString status = QString("Заполнено комплектов: %1 из %2").arg(filled).arg(m_expectedCount);

    auto findDups = [](const QStringList& lst, const QString& label, QString& statusText) {
        QStringList dup;
        QSet<QString> seen;
        for (const QString& v : lst) {
            if (v.isEmpty())
                continue;
            if (seen.contains(v)) {
                dup.append(v);
            } else {
                seen.insert(v);
            }
        }
        if (!dup.isEmpty())
            statusText += QString(" · Дубли %1: %2").arg(label, dup.join("; "));
    };
    findDups(s, "SN", status);
    findDups(i1, "IMEI 1", status);
    findDups(i2, "IMEI 2", status);

    int bad = 0;
    for (const QString& v : i1) {
        if (!v.isEmpty() && v.size() != 15)
            ++bad;
    }
    for (const QString& v : i2) {
        if (!v.isEmpty() && v.size() != 15)
            ++bad;
    }
    if (bad > 0)
        status += QString(" · Неверная длина IMEI: %1 стр.").arg(bad);

    ui->labelStatus->setText(status);
}

void SerialUnitsDialog::onCellChanged(int row, int col)
{
    if (m_populating)
        return;

    QTableWidgetItem* it = ui->tableWidget->item(row, col);
    if (it && !it->text().trimmed().isEmpty())
        moveToNextCell(row, col);
    updateStatus();
}

void SerialUnitsDialog::moveToNextCell(int row, int col)
{
    if (col < 2) {
        ui->tableWidget->setCurrentCell(row, col + 1);
    } else if (row + 1 < ui->tableWidget->rowCount()) {
        ui->tableWidget->setCurrentCell(row + 1, 0);
    }
}

void SerialUnitsDialog::onAccepted()
{
    const QStringList s = serials();
    const QStringList i1 = imei1();
    const QStringList i2 = imei2();

    int filled = 0;
    for (const QString& v : s) {
        if (!v.isEmpty())
            ++filled;
    }
    if (filled != m_expectedCount) {
        QMessageBox::warning(this, "Серийные номера",
                             QString("Заполнено комплектов: %1 из %2. Заполните серийные номера во всех.")
                                 .arg(filled)
                                 .arg(m_expectedCount));
        return;
    }

    auto hasDups = [](const QStringList& lst, QString* which) {
        QSet<QString> seen;
        for (const QString& v : lst) {
            if (v.isEmpty())
                continue;
            if (seen.contains(v)) {
                *which = v;
                return true;
            }
            seen.insert(v);
        }
        return false;
    };

    QString dup;
    if (hasDups(s, &dup)) {
        QMessageBox::warning(this, "Серийные номера", "Серийный номер повторяется: " + dup);
        return;
    }
    if (hasDups(i1, &dup)) {
        QMessageBox::warning(this, "IMEI 1", "IMEI 1 повторяется: " + dup);
        return;
    }
    if (hasDups(i2, &dup)) {
        QMessageBox::warning(this, "IMEI 2", "IMEI 2 повторяется: " + dup);
        return;
    }

    int bad = 0;
    for (const QString& v : i1) {
        if (!v.isEmpty() && v.size() != 15)
            ++bad;
    }
    for (const QString& v : i2) {
        if (!v.isEmpty() && v.size() != 15)
            ++bad;
    }
    if (bad > 0) {
        QMessageBox::warning(this, "IMEI", "IMEI должен содержать ровно 15 цифр. Проверьте значения IMEI.");
        return;
    }

    accept();
}