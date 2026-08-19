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

ReceiptForm::ReceiptForm(QWidget* parent) : DocumentDialog(parent), ui(new Ui::ReceiptForm)
{
    ui->setupUi(this);
    setWindowTitle("Документ: Поступление терминалов");
    resize(900, 600);

    // Дата по умолчанию — сегодня.
    ui->dateEdit->setDate(QDate::currentDate());

    // Номер документа генерируется при проведении (не здесь), чтобы не
    // сжигать значения последовательности для отменённых форм.

    // Табличная часть в стиле 1С: строка = модель + кол-во + комплекты
    // «серийный номер и его IMEI 1 / IMEI 2» (колонка ColSerials).
    rowsModel = new QStandardItemModel(0, 3, this);
    rowsModel->setHorizontalHeaderLabels({"Модель", "Кол-во", "Серийные номера (SN · IMEI 1 · IMEI 2)"});
    ui->tableView->setModel(rowsModel);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
    ui->tableView->setColumnWidth(ColModel, 180);
    ui->tableView->setColumnWidth(ColQty, 60);

    // Двойной клик по ячейке комплектов открывает окно ввода.
    connect(ui->tableView, &QTableView::doubleClicked, this, &ReceiptForm::onTableViewDoubleClicked);

    // Колонка комплектов только для чтения: вместо редактора ячейки — окно ввода.
    ui->tableView->setItemDelegateForColumn(ColSerials, new ReadOnlyDelegate(this));

    // Загружаем модели для комбобокса в ячейке.
    loadModelsToDelegate();

    // F9 для дублирования строки.
    ui->tableView->installEventFilter(this);

    // Сканер штрих-кода: перехват ввода по всей форме.
    m_scanner = new BarcodeScanner(this);
    connect(m_scanner, &BarcodeScanner::scanFinished, this, &ReceiptForm::onScanFinished);
    qApp->installEventFilter(this);

    // Сканер штрих-кода через COM-порт (типовые USB-сканеры в режиме RS-232).
    setupSerialScanner();

    QTimer::singleShot(0, this, [this]() { ui->tableView->setFocus(); });
}

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

ReceiptForm::~ReceiptForm()
{
    qApp->removeEventFilter(this);
    delete ui;
}

QString ReceiptForm::docType() const
{
    return "receipt";
}

QLineEdit* ReceiptForm::headerNumberEdit() const
{
    return ui->lineEditNumber;
}

QDateEdit* ReceiptForm::headerDateEdit() const
{
    return ui->dateEdit;
}

QTextEdit* ReceiptForm::headerCommentEdit() const
{
    return ui->textEditComment;
}

QTableView* ReceiptForm::tableView() const
{
    return ui->tableView;
}

void ReceiptForm::loadModelsToDelegate()
{
    m_models.clear();
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    if (!query.exec("SELECT modelid, modelname FROM tblmodels ORDER BY modelname")) {
        qCWarning(logSQL) << "Failed to load models:" << query.lastError().text();
        return;
    }

    while (query.next()) {
        m_models.append(qMakePair(query.value(0).toInt(), query.value(1).toString()));
    }

    ui->tableView->setItemDelegateForColumn(ColModel, new ComboBoxDelegate(m_models, this));
}

void ReceiptForm::loadSpecificEditData(int docId)
{
    const QSqlDatabase& db = DatabaseManager::instance().getDatabase();
    DocumentRepository documents(db);

    // Заголовок документа.
    const models::DocumentHeader header = documents.loadHeader(DocumentRepository::Receipt, docId);
    if (header.id != 0) {
        ui->lineEditNumber->setText(header.docNumber);
        ui->lineEditNumber->setReadOnly(true);
        ui->dateEdit->setDate(header.date);
        ui->textEditComment->setText(header.comments);
    }

    // Строки-«исходник». Для документов, созданных до миграции 012,
    // их нет — восстанавливаем группировкой развёрнутых терминалов по модели.
    QVector<models::ReceiptItem> items = documents.loadReceiptItems(docId);
    if (items.isEmpty()) {
        const auto rows = documents.loadReceiptRows(docId);
        for (const auto& r : rows) {
            int idx = -1;
            for (int k = 0; k < items.size(); ++k) {
                if (items.at(k).modelId == r.modelId) {
                    idx = k;
                    break;
                }
            }
            if (idx < 0) {
                models::ReceiptItem it;
                it.modelId = r.modelId;
                it.modelName = r.modelName;
                it.qty = 0;
                items.append(it);
                idx = items.size() - 1;
            }
            models::ReceiptSerial s;
            s.linenum = items[idx].serials.size() + 1;
            s.serialNumber = r.serialNumber;
            s.imei1 = r.imei1;
            s.imei2 = r.imei2;
            items[idx].serials.append(s);
            items[idx].qty = items[idx].serials.size();
        }
    }

    for (const auto& item : items) {
        int r = rowsModel->rowCount();
        rowsModel->insertRow(r);

        QStandardItem* modelItem = new QStandardItem(item.modelName);
        modelItem->setData(item.modelId, Qt::UserRole);
        rowsModel->setItem(r, ColModel, modelItem);
        rowsModel->setItem(r, ColQty, new QStandardItem(QString::number(item.qty)));

        QStringList serials, imei1, imei2;
        for (const auto& s : item.serials) {
            serials << s.serialNumber;
            imei1 << s.imei1;
            imei2 << s.imei2;
        }
        setUnitsForRow(r, serials, imei1, imei2);
        refreshRow(r);
    }

    setWindowTitle(QString("Редактирование поступления ID %1").arg(docId));
}

void ReceiptForm::on_btnAddRow_clicked()
{
    int row = appendRowWithFirstModel();
    rowsModel->setItem(row, ColSerials, new QStandardItem());
    refreshRow(row);

    ui->tableView->setCurrentIndex(rowsModel->index(row, ColSerials));
}

int ReceiptForm::appendRowWithFirstModel()
{
    int row = rowsModel->rowCount();
    rowsModel->insertRow(row);

    QString modelName;
    int modelId = 0;
    if (!m_models.isEmpty()) {
        modelId = m_models.first().first;
        modelName = m_models.first().second;
    }

    QStandardItem* modelItem = new QStandardItem(modelName);
    modelItem->setData(modelId, Qt::UserRole);
    rowsModel->setItem(row, ColModel, modelItem);
    rowsModel->setItem(row, ColQty, new QStandardItem("1"));
    return row;
}

void ReceiptForm::on_btnDeleteRow_clicked()
{
    int row = ui->tableView->currentIndex().row();
    if (row >= 0) {
        rowsModel->removeRow(row);
    }
}

int ReceiptForm::rowQty(int row) const
{
    QStandardItem* item = rowsModel->item(row, ColQty);
    if (!item)
        return 1;
    bool ok = false;
    const int v = item->text().toInt(&ok);
    return (ok && v > 0) ? v : 1;
}

QStringList ReceiptForm::serialsForRow(int row) const
{
    QStandardItem* item = rowsModel->item(row, ColSerials);
    return item ? item->data(RoleSerials).toStringList() : QStringList();
}

QStringList ReceiptForm::imei1ForRow(int row) const
{
    QStandardItem* item = rowsModel->item(row, ColSerials);
    return item ? item->data(RoleImei1).toStringList() : QStringList();
}

QStringList ReceiptForm::imei2ForRow(int row) const
{
    QStandardItem* item = rowsModel->item(row, ColSerials);
    return item ? item->data(RoleImei2).toStringList() : QStringList();
}

void ReceiptForm::setUnitsForRow(int row, const QStringList& serials, const QStringList& imei1, const QStringList& imei2)
{
    QStandardItem* item = rowsModel->item(row, ColSerials);
    if (!item) {
        item = new QStandardItem();
        rowsModel->setItem(row, ColSerials, item);
    }
    item->setData(serials, RoleSerials);
    item->setData(imei1, RoleImei1);
    item->setData(imei2, RoleImei2);
}

void ReceiptForm::refreshRow(int row)
{
    QStandardItem* item = rowsModel->item(row, ColSerials);
    if (item)
        item->setText(SerialUnitsService::summary(serialsForRow(row), rowQty(row)));
}

void ReceiptForm::openUnitsDialog(int row)
{
    if (row < 0)
        return;

    SerialUnitsDialog dlg(rowQty(row), serialsForRow(row), imei1ForRow(row), imei2ForRow(row),
                          "Серийные номера: комплекты (SN · IMEI 1 · IMEI 2)", this);
    m_unitsDialog = &dlg;
    const int result = dlg.exec();
    m_unitsDialog = nullptr;
    if (result == QDialog::Accepted) {
        setUnitsForRow(row, dlg.serials(), dlg.imei1(), dlg.imei2());
        refreshRow(row);
    }
}

void ReceiptForm::onTableViewDoubleClicked(const QModelIndex& index)
{
    if (!index.isValid())
        return;
    if (index.column() == ColSerials)
        openUnitsDialog(index.row());
}

void ReceiptForm::on_btnPost_clicked()
{
    executePost();
}
void ReceiptForm::onPostSuccess(int docId)
{
    PostActionLogger::log("POST", "tblreceiptdocs", docId);

    QMessageBox::information(this, "Успех", "Документ успешно проведен!");
    PostActionLogger::notify();
    this->close();
}

void ReceiptForm::on_btnPrint_clicked()
{
    if (!ensureDocNumber())
        return;

    QString html = PrintService::docHeader();

    html += "<h2>ПРИХОДНАЯ НАКЛАДНАЯ № " + ui->lineEditNumber->text().toHtmlEscaped() + "</h2>";
    html += "<p>от " + ui->dateEdit->date().toString("dd.MM.yyyy") + " г.</p>";
    html += "<p><b>Поставщик:</b> ООО «POC Terminal»</p>";

    QString comment = ui->textEditComment->toPlainText().trimmed();
    if (!comment.isEmpty())
        html += "<p><b>Комментарий:</b> " + comment.toHtmlEscaped() + "</p>";

    html += "<table><tr><th>№</th><th>Модель</th><th>Серийный номер</th><th>IMEI 1</th><th>IMEI 2</th></tr>";

    // Печатаем комплекты построчно: серийный номер и его IMEI в одной строке —
    // привязка видна и в печатной форме.
    int num = 0;
    for (int i = 0; i < rowsModel->rowCount(); ++i) {
        const QString model = rowsModel->data(rowsModel->index(i, ColModel)).toString();
        const QStringList serials = serialsForRow(i);
        const QStringList imei1 = imei1ForRow(i);
        const QStringList imei2 = imei2ForRow(i);

        for (int k = 0; k < serials.size(); ++k) {
            if (serials.at(k).trimmed().isEmpty())
                continue;
            ++num;
            html += "<tr><td>" + QString::number(num) +
                    "</td>"
                    "<td>" +
                    model.toHtmlEscaped() +
                    "</td>"
                    "<td>" +
                    serials.at(k).trimmed().toHtmlEscaped() +
                    "</td>"
                    "<td>" +
                    (k < imei1.size() ? imei1.at(k).trimmed().toHtmlEscaped() : QString("&nbsp;")) +
                    "</td>"
                    "<td>" +
                    (k < imei2.size() ? imei2.at(k).trimmed().toHtmlEscaped() : QString("&nbsp;")) + "</td></tr>";
        }
    }
    html += "</table>";

    html += "<p style='margin-top: 40px;'>Принял: ________________ / ____________</p>";
    html += PrintService::docFooter();

    PrintService::printHtml(html, this);
}

void ReceiptForm::on_btnClose_clicked()
{
    close();
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
