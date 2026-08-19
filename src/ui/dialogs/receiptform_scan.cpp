#include "receiptform.h"
#include "ui_receiptform.h"
#include "database/databasemanager.h"
#include "database/repositories/documentrepository.h"
#include "database/repositories/terminalrepository.h"
#include "utils/barcodeparser.h"
#include "utils/barcodescanner.h"
#include "utils/serialscanner.h"
#include "ui/delegates/comboboxdelegate.h"
#include "ui/delegates/readonlydelegate.h"
#include "ui/dialogs/serialunitsdialog.h"
#include <QApplication>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QTime>
#include <QDebug>
#include <QKeyEvent>
#include <QLineEdit>
#include <QComboBox>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QTableView>
#include <QHeaderView>
#include <QSet>
#include "utils/logging.h"
#include "services/documentnumbergenerator.h"
#include "services/postactionlogger.h"
#include "services/serialunitsservice.h"
#include "ui/base/printservice.h"
#include "ui/base/transactionguard.h"
#include <QJsonObject>
#include <QTimer>

void ReceiptForm::setupSerialScanner()
{
    const QJsonObject cfg = DatabaseManager::instance().configObject()["scanner"].toObject();
    if (!cfg.value("enabled").toBool(true)) {
        ui->labelScannerStatus->setText("Сканер: отключен в config.json");
        return;
    }

    const QString port = cfg.value("port").toString("COM8");
    const int baud = cfg.value("baud_rate").toInt(9600);

    m_serialScanner = new SerialScanner(this);
    connect(m_serialScanner, &SerialScanner::scanFinished, this, &ReceiptForm::onScanFinished);
    if (!m_serialScanner->start(port, baud)) {
        ui->labelScannerStatus->setText(QString("Сканер: не удалось открыть %1 (%2 бод) — проверьте, что порт свободен")
                                            .arg(port)
                                            .arg(baud));
        qCWarning(logApp) << "Сканер: не удалось открыть" << port;
    } else {
        ui->labelScannerStatus->setText(QString("Сканер: %1 (%2 бод) — готов").arg(port).arg(baud));
    }
}

bool ReceiptForm::canAcceptScan() const
{
    // Окно комплектов открыто — скан принимаем, если фокус не в текстовом поле
    // этого окна (сканер пишет прямо в его таблицу).
    if (m_unitsDialog && m_unitsDialog->isActiveWindow()) {
        QWidget* focus = QApplication::focusWidget();
        if (!focus)
            return true;
        if (qobject_cast<QLineEdit*>(focus) || qobject_cast<QTextEdit*>(focus) ||
            qobject_cast<QPlainTextEdit*>(focus) || qobject_cast<QComboBox*>(focus))
            return false;
        return focus->window() == m_unitsDialog->window();
    }

    if (!isActiveWindow())
        return false;
    QWidget* focus = QApplication::focusWidget();
    if (!focus)
        return true;
    // Не перехватываем ввод в текстовых редакторах (в т.ч. редакторе ячейки),
    // комбобоксах и поле ввода комментария.
    if (qobject_cast<QLineEdit*>(focus) || qobject_cast<QTextEdit*>(focus) ||
        qobject_cast<QPlainTextEdit*>(focus) || qobject_cast<QComboBox*>(focus))
        return false;
    return focus->window() == window();
}

void ReceiptForm::onScanFinished(const QString& raw)
{
    // Если окно комплектов открыто — скан обрабатывает оно, а не форма.
    if (m_unitsDialog) {
        m_unitsDialog->handleScan(raw);
        return;
    }

    const BarcodeScan data = BarcodeParser::parse(raw);
    if (!data.hasData())
        return;

    int row = ui->tableView->currentIndex().row();
    if (row < 0)
        row = appendRowWithFirstModel();

    // Комплекты хранятся как три параллельных списка: imei[n] принадлежит
    // serial[n]. Логика «SN → IMEI 1 → IMEI 2 → следующий SN» в BarcodeParser:
    // серийник открывает новый комплект, голый IMEI дописывается к последнему
    // (сначала в IMEI 1, затем в IMEI 2).
    QStringList s = serialsForRow(row);
    QStringList i1 = imei1ForRow(row);
    QStringList i2 = imei2ForRow(row);
    if (!BarcodeParser::applyScan(s, i1, i2, data))
        return;

    setUnitsForRow(row, s, i1, i2);
    refreshRow(row);
    ui->tableView->setCurrentIndex(rowsModel->index(row, ColSerials));
    ui->tableView->setFocus();
}

bool ReceiptForm::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == ui->tableView && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_F9) {
            int row = ui->tableView->currentIndex().row();
            if (row < 0)
                return true;

            int newRow = rowsModel->rowCount();
            rowsModel->insertRow(newRow);

            for (int col = 0; col < rowsModel->columnCount(); ++col) {
                QModelIndex src = rowsModel->index(row, col);
                QStandardItem* newItem = rowsModel->itemFromIndex(src)->clone();
                rowsModel->setItem(newRow, col, newItem);
            }

            ui->tableView->setCurrentIndex(rowsModel->index(newRow, 0));
            return true;
        }
    }

    // Сканер штрих-кода: обрабатываем нажатия только если форма активна
    // и фокус не «сидит» в текстовом поле.
    if (m_scanner && event->type() == QEvent::KeyPress && canAcceptScan()) {
        QKeyEvent* key = static_cast<QKeyEvent*>(event);
        const int k = key->key();
        if (k == Qt::Key_Return || k == Qt::Key_Enter || k == Qt::Key_Tab) {
            if (m_scanner->feedTerminator())
                return true; // терминатор сканера — не нажимаем кнопку/не уводим фокус
        } else {
            const QString text = key->text();
            // Символы штрих-кода — ASCII (до 0xFF). Кириллицу ручного ввода не трогаем.
            if (!text.isEmpty()) {
                bool isAscii = true;
                for (QChar ch : text) {
                    if (ch.unicode() > 0xFF) {
                        isAscii = false;
                        break;
                    }
                }
                if (isAscii) {
                    m_scanner->feed(text);
                    if (m_scanner->isActive())
                        return true; // поглощаем нажатия активного буста
                }
            }
        }
    }
    return QDialog::eventFilter(obj, event);
}
