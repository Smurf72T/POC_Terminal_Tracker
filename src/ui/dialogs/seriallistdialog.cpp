#include "seriallistdialog.h"
#include "ui_seriallistdialog.h"

#include <QMessageBox>
#include <QRegularExpression>
#include <QSet>

SerialListDialog::SerialListDialog(Mode mode, int expectedCount, bool strictCount, const QStringList& values,
                                   const QString& title, QWidget* parent)
    : QDialog(parent), ui(new Ui::SerialListDialog), m_mode(mode), m_expectedCount(expectedCount),
      m_strictCount(strictCount)
{
    ui->setupUi(this);
    setWindowTitle(title);
    resize(420, 380);

    if (m_mode == Serial) {
        ui->labelInfo->setText(QString("По одному серийному номеру на строку. Нужно ввести: %1.").arg(m_expectedCount));
    } else {
        ui->labelInfo->setText(
            QString("По одному IMEI на строку (15 цифр, разделители уберутся сами). Ожидается: %1.").arg(m_expectedCount));
    }

    ui->plainTextEdit->setPlainText(values.join('\n'));

    connect(ui->plainTextEdit, &QPlainTextEdit::textChanged, this, &SerialListDialog::updateStatus);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &SerialListDialog::onAccepted);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &SerialListDialog::reject);

    updateStatus();
}

SerialListDialog::~SerialListDialog()
{
    delete ui;
}

QStringList SerialListDialog::parseLines() const
{
    QStringList result;
    const QStringList raw = ui->plainTextEdit->toPlainText().split('\n');

    static const QRegularExpression digitRe("[^\\d]");
    for (const QString& line : raw) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty())
            continue;
        if (m_mode == Imei) {
            QString digits = trimmed;
            digits.remove(digitRe);
            if (!digits.isEmpty())
                result.append(digits);
        } else {
            result.append(trimmed);
        }
    }
    return result;
}

QStringList SerialListDialog::values() const
{
    return parseLines();
}

void SerialListDialog::updateStatus()
{
    const QStringList lines = parseLines();
    const int count = lines.size();

    QString status;
    if (m_mode == Serial) {
        if (count >= m_expectedCount) {
            status = QString("Введено: %1 из %2").arg(count).arg(m_expectedCount);
        } else {
            status = QString("Введено: %1 из %2 (не хватает %3)")
                         .arg(count)
                         .arg(m_expectedCount)
                         .arg(m_expectedCount - count);
        }
    } else {
        status = QString("Введено: %1").arg(count);
    }

    QStringList dup;
    QSet<QString> seen;
    for (const QString& v : lines) {
        if (seen.contains(v)) {
            dup.append(v);
        } else {
            seen.insert(v);
        }
    }
    if (!dup.isEmpty())
        status += QString(" · Дубли: %1").arg(dup.join("; "));

    if (m_mode == Imei) {
        int bad = 0;
        for (const QString& v : lines) {
            if (v.size() != 15)
                ++bad;
        }
        if (bad > 0)
            status += QString(" · Неверная длина: %1 стр.").arg(bad);
    }

    ui->labelStatus->setText(status);
}

void SerialListDialog::onAccepted()
{
    const QStringList lines = parseLines();

    if (m_mode == Serial && m_strictCount && lines.size() != m_expectedCount) {
        QMessageBox::warning(this, "Серийные номера",
                             QString("Введено %1 серийных номеров из %2. Заполните все.")
                                 .arg(lines.size())
                                 .arg(m_expectedCount));
        return;
    }

    if (m_mode == Serial && m_strictCount) {
        QSet<QString> seen;
        for (const QString& v : lines) {
            if (seen.contains(v)) {
                QMessageBox::warning(this, "Серийные номера", QString("Серийный номер повторяется: %1").arg(v));
                return;
            }
            seen.insert(v);
        }
    }

    if (m_mode == Imei) {
        QStringList badLines;
        QSet<QString> seen;
        for (int i = 0; i < lines.size(); ++i) {
            if (lines.at(i).size() != 15) {
                badLines << QString::number(i + 1);
            }
            if (seen.contains(lines.at(i))) {
                QMessageBox::warning(this, "IMEI", QString("IMEI повторяется: %1").arg(lines.at(i)));
                return;
            }
            seen.insert(lines.at(i));
        }
        if (!badLines.isEmpty()) {
            QMessageBox::warning(this, "IMEI", QString("IMEI должен содержать ровно 15 цифр. Строки: %1")
                                                    .arg(badLines.join(", ")));
            return;
        }
    }

    accept();
}