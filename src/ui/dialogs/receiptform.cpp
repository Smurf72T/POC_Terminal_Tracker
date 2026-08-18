#include "receiptform.h"
#include "ui_receiptform.h"
#include "database/databasemanager.h"
#include "database/repositories/documentrepository.h"
#include "utils/barcodeparser.h"
#include "utils/barcodescanner.h"
#include "utils/serialscanner.h"
#include "ui/delegates/comboboxdelegate.h"
#include "ui/delegates/readonlydelegate.h"
#include "ui/dialogs/seriallistdialog.h"
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
#include <QJsonObject>
#include <QPrinter>
#include <QPrintDialog>
#include <QTextDocument>
#include <QTimer>

ReceiptForm::ReceiptForm(QWidget* parent) : QDialog(parent), ui(new Ui::ReceiptForm)
{
    ui->setupUi(this);
    setWindowTitle("Документ: Поступление терминалов");
    resize(900, 600);

    // Дата по умолчанию — сегодня.
    ui->dateEdit->setDate(QDate::currentDate());

    // Номер документа генерируется при проведении (не здесь), чтобы не
    // сжигать значения последовательности для отменённых форм.

    // Табличная часть в стиле 1С: строка = модель + кол-во + списки серийников/IMEI.
    rowsModel = new QStandardItemModel(0, 5, this);
    rowsModel->setHorizontalHeaderLabels({"Модель", "Кол-во", "Серийные номера", "IMEI 1", "IMEI 2"});
    ui->tableView->setModel(rowsModel);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
    ui->tableView->setColumnWidth(ColModel, 180);
    ui->tableView->setColumnWidth(ColQty, 60);
    ui->tableView->setColumnWidth(ColSerial, 220);
    ui->tableView->setColumnWidth(ColImei1, 200);
    ui->tableView->setColumnWidth(ColImei2, 200);

    // Двойной клик по колонке-списку открывает окно ввода.
    connect(ui->tableView, &QTableView::doubleClicked, this, &ReceiptForm::onTableViewDoubleClicked);

    // Колонки-списки только для чтения: вместо редактора ячейки открываем окно ввода.
    for (int col = ColSerial; col <= ColImei2; ++col)
        ui->tableView->setItemDelegateForColumn(col, new ReadOnlyDelegate(this));

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
    if (!cfg.value("enabled").toBool(true))
        return;

    const QString port = cfg.value("port").toString("COM8");
    const int baud = cfg.value("baud_rate").toInt(9600);

    m_serialScanner = new SerialScanner(this);
    connect(m_serialScanner, &SerialScanner::scanFinished, this, &ReceiptForm::onScanFinished);
    if (!m_serialScanner->start(port, baud)) {
        qCWarning(logApp) << "Сканер: не удалось открыть" << port;
    }
}

ReceiptForm::~ReceiptForm()
{
    qApp->removeEventFilter(this);
    delete ui;
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

void ReceiptForm::loadForEdit(int docId)
{
    m_editMode = true;
    m_editDocId = docId;

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
        setListForRow(r, ColSerial, serials);
        setListForRow(r, ColImei1, imei1);
        setListForRow(r, ColImei2, imei2);
        refreshRow(r);
    }

    setWindowTitle(QString("Редактирование поступления ID %1").arg(docId));
}

void ReceiptForm::on_btnAddRow_clicked()
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
    rowsModel->setItem(row, ColSerial, new QStandardItem());
    rowsModel->setItem(row, ColImei1, new QStandardItem());
    rowsModel->setItem(row, ColImei2, new QStandardItem());
    refreshRow(row);

    ui->tableView->setCurrentIndex(rowsModel->index(row, ColSerial));
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

QStringList ReceiptForm::listForRow(int row, int col) const
{
    QStandardItem* item = rowsModel->item(row, col);
    return item ? item->data(ListRole).toStringList() : QStringList();
}

void ReceiptForm::setListForRow(int row, int col, const QStringList& values)
{
    QStandardItem* item = rowsModel->item(row, col);
    if (!item) {
        item = new QStandardItem();
        rowsModel->setItem(row, col, item);
    }
    item->setData(values, ListRole);
}

QString ReceiptForm::listSummary(const QStringList& values, int expected) const
{
    if (values.isEmpty())
        return "—";
    QString preview = values.join("; ");
    if (preview.size() > 34)
        preview = preview.left(34) + "…";
    return QString("%1/%2 · %3").arg(values.size()).arg(expected).arg(preview);
}

void ReceiptForm::refreshRow(int row)
{
    const int qty = rowQty(row);
    for (int col = ColSerial; col <= ColImei2; ++col) {
        QStandardItem* item = rowsModel->item(row, col);
        if (item)
            item->setText(listSummary(listForRow(row, col), qty));
    }
}

void ReceiptForm::openListDialog(int row, int col)
{
    if (col < ColSerial || col > ColImei2)
        return;

    const int qty = rowQty(row);
    const bool imei = col >= ColImei1;
    const SerialListDialog::Mode mode = imei ? SerialListDialog::Imei : SerialListDialog::Serial;
    const QString title = col == ColSerial ? "Серийные номера" : (col == ColImei1 ? "IMEI 1" : "IMEI 2");

    SerialListDialog dlg(mode, qty, !imei, listForRow(row, col), title, this);
    if (dlg.exec() == QDialog::Accepted) {
        setListForRow(row, col, dlg.values());
        refreshRow(row);
    }
}

void ReceiptForm::onTableViewDoubleClicked(const QModelIndex& index)
{
    if (!index.isValid())
        return;
    openListDialog(index.row(), index.column());
}

void ReceiptForm::on_btnPost_clicked()
{
    if (rowsModel->rowCount() == 0) {
        QMessageBox::warning(this, "Внимание", "Добавьте хотя бы одну строку!");
        return;
    }

    // 0. Собираем строки и проверяем дубли в рамках документа.
    struct ItemData {
        int modelId;
        int qty;
        QStringList serials;
        QStringList imei1;
        QStringList imei2;
    };
    QList<ItemData> items;
    QSet<QString> usedSerials, usedImei1, usedImei2;

    for (int r = 0; r < rowsModel->rowCount(); ++r) {
        ItemData it;
        it.modelId = rowsModel->data(rowsModel->index(r, ColModel), Qt::UserRole).toInt();
        it.qty = rowQty(r);
        it.serials = listForRow(r, ColSerial);
        it.imei1 = listForRow(r, ColImei1);
        it.imei2 = listForRow(r, ColImei2);

        if (it.modelId <= 0) {
            QMessageBox::critical(this, "Ошибка", QString("Строка %1: выберите модель из списка.").arg(r + 1));
            return;
        }
        if (it.serials.size() != it.qty) {
            QMessageBox::critical(this, "Ошибка",
                                  QString("Строка %1: введено %2 серийных номеров из %3.")
                                      .arg(r + 1)
                                      .arg(it.serials.size())
                                      .arg(it.qty));
            return;
        }
        if (!it.imei1.isEmpty() && it.imei1.size() != it.qty) {
            QMessageBox::critical(this, "Ошибка",
                                  QString("Строка %1: введено %2 значений IMEI 1 из %3.")
                                      .arg(r + 1)
                                      .arg(it.imei1.size())
                                      .arg(it.qty));
            return;
        }
        if (!it.imei2.isEmpty() && it.imei2.size() != it.qty) {
            QMessageBox::critical(this, "Ошибка",
                                  QString("Строка %1: введено %2 значений IMEI 2 из %3.")
                                      .arg(r + 1)
                                      .arg(it.imei2.size())
                                      .arg(it.qty));
            return;
        }

        for (const QString& sn : it.serials) {
            if (usedSerials.contains(sn)) {
                QMessageBox::critical(this, "Ошибка",
                                      QString("Серийный номер повторяется в документе: %1").arg(sn));
                return;
            }
            usedSerials.insert(sn);
        }
        for (const QString& im : it.imei1) {
            if (im.isEmpty())
                continue;
            if (im.size() != 15) {
                QMessageBox::critical(this, "Ошибка",
                                      QString("Строка %1: IMEI 1 должен содержать ровно 15 цифр (сейчас: %2)")
                                          .arg(r + 1)
                                          .arg(im));
                return;
            }
            if (usedImei1.contains(im)) {
                QMessageBox::critical(this, "Ошибка", QString("IMEI 1 повторяется в документе: %1").arg(im));
                return;
            }
            usedImei1.insert(im);
        }
        for (const QString& im : it.imei2) {
            if (im.isEmpty())
                continue;
            if (im.size() != 15) {
                QMessageBox::critical(this, "Ошибка",
                                      QString("Строка %1: IMEI 2 должен содержать ровно 15 цифр (сейчас: %2)")
                                          .arg(r + 1)
                                          .arg(im));
                return;
            }
            if (usedImei2.contains(im)) {
                QMessageBox::critical(this, "Ошибка", QString("IMEI 2 повторяется в документе: %1").arg(im));
                return;
            }
            usedImei2.insert(im);
        }

        items.append(it);
    }

    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    if (!db.transaction()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось начать транзакцию");
        return;
    }

    QSqlQuery query(db);
    int docId;

    // 1. Шапка документа.
    if (m_editMode) {
        query.prepare("UPDATE tblreceiptdocs SET docdate = :date, comments = :comm WHERE receiptdocid = :id");
        query.bindValue(":date", QDateTime(ui->dateEdit->date(), QTime::currentTime()));
        query.bindValue(":comm", ui->textEditComment->toPlainText());
        query.bindValue(":id", m_editDocId);

        if (!query.exec()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД", "Не удалось обновить шапку: " + query.lastError().text());
            return;
        }
        docId = m_editDocId;
    } else {
        if (ui->lineEditNumber->text().trimmed().isEmpty()) {
            QString num = DatabaseManager::instance().generateDocNumber("receipt");
            if (num.isEmpty()) {
                db.rollback();
                QMessageBox::critical(this, "Ошибка БД", "Не удалось сгенерировать номер документа.");
                return;
            }
            ui->lineEditNumber->setText(num);
        }
        query.prepare("INSERT INTO tblreceiptdocs (docnumber, docdate, comments) "
                      "VALUES (:num, :date, :comm) RETURNING receiptdocid");
        query.bindValue(":num", ui->lineEditNumber->text());
        query.bindValue(":date", QDateTime(ui->dateEdit->date(), QTime::currentTime()));
        query.bindValue(":comm", ui->textEditComment->toPlainText());

        if (!query.exec() || !query.next()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД", "Не удалось создать шапку: " + query.lastError().text());
            return;
        }
        docId = query.value(0).toInt();
    }

    // 2. В режиме редактирования пересоздаём «исходник» и связи.
    if (m_editMode) {
        QSqlQuery delItems(db);
        delItems.prepare("DELETE FROM tblreceiptitems WHERE receiptdocid = :id");
        delItems.bindValue(":id", docId);
        if (!delItems.exec()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД",
                                  "Не удалось удалить старые строки: " + delItems.lastError().text());
            return;
        }

        QSqlQuery delDetails(db);
        delDetails.prepare("DELETE FROM tblreceiptdetails WHERE receiptdocid = :id");
        delDetails.bindValue(":id", docId);
        if (!delDetails.exec()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД",
                                  "Не удалось удалить старые связи: " + delDetails.lastError().text());
            return;
        }
    }

    // 3. Разворачиваем строки в терминалы и сохраняем «исходник».
    for (const ItemData& it : items) {
        QSqlQuery itemQuery(db);
        itemQuery.prepare("INSERT INTO tblreceiptitems (receiptdocid, modelid, qty) "
                          "VALUES (:did, :mid, :qty) RETURNING receiptitemid");
        itemQuery.bindValue(":did", docId);
        itemQuery.bindValue(":mid", it.modelId);
        itemQuery.bindValue(":qty", it.qty);
        if (!itemQuery.exec() || !itemQuery.next()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка БД",
                                  "Не удалось сохранить строку документа: " + itemQuery.lastError().text());
            return;
        }
        const int itemId = itemQuery.value(0).toInt();

        for (int i = 0; i < it.serials.size(); ++i) {
            const QString serial = it.serials.at(i);
            const QString im1 = i < it.imei1.size() ? it.imei1.at(i) : QString();
            const QString im2 = i < it.imei2.size() ? it.imei2.at(i) : QString();

            QSqlQuery serialQuery(db);
            serialQuery.prepare("INSERT INTO tblreceiptserials (receiptitemid, linenum, serialnumber, imei1, imei2) "
                                "VALUES (:iid, :ln, :sn, :i1, :i2)");
            serialQuery.bindValue(":iid", itemId);
            serialQuery.bindValue(":ln", i + 1);
            serialQuery.bindValue(":sn", serial);
            serialQuery.bindValue(":i1", im1);
            serialQuery.bindValue(":i2", im2);
            if (!serialQuery.exec()) {
                db.rollback();
                QMessageBox::critical(this, "Ошибка БД",
                                      QString("Ошибка сохранения серийного номера %1:\n%2")
                                          .arg(serial, serialQuery.lastError().text()));
                return;
            }

            int newTermId;
            QSqlQuery findQuery(db);
            findQuery.prepare("SELECT terminalid FROM tblterminals WHERE serialnumber = :sn");
            findQuery.bindValue(":sn", serial);
            const bool found = findQuery.exec() && findQuery.next();

            if (found && m_editMode) {
                // Редактирование: серийник уже был в базе — обновляем терминал.
                newTermId = findQuery.value(0).toInt();
                QSqlQuery termQuery(db);
                termQuery.prepare("UPDATE tblterminals SET modelid = :mid, imei1 = :i1, imei2 = :i2 "
                                  "WHERE terminalid = :tid");
                termQuery.bindValue(":mid", it.modelId);
                termQuery.bindValue(":i1", im1);
                termQuery.bindValue(":i2", im2);
                termQuery.bindValue(":tid", newTermId);
                if (!termQuery.exec()) {
                    db.rollback();
                    QMessageBox::critical(this, "Ошибка БД",
                                          QString("Ошибка при обновлении терминала %1:\n%2")
                                              .arg(serial, termQuery.lastError().text()));
                    return;
                }
            } else if (found) {
                // Новый документ: серийник уже занят в базе.
                db.rollback();
                QMessageBox::critical(this, "Ошибка",
                                      QString("Серийный номер уже есть в базе: %1").arg(serial));
                return;
            } else {
                QSqlQuery termQuery(db);
                termQuery.prepare("INSERT INTO tblterminals (serialnumber, modelid, imei1, imei2, status) "
                                  "VALUES (:sn, :mid, :i1, :i2, 0) RETURNING terminalid");
                termQuery.bindValue(":sn", serial);
                termQuery.bindValue(":mid", it.modelId);
                termQuery.bindValue(":i1", im1);
                termQuery.bindValue(":i2", im2);
                if (!termQuery.exec() || !termQuery.next()) {
                    db.rollback();
                    QMessageBox::critical(this, "Ошибка БД",
                                          QString("Ошибка при добавлении терминала %1:\n%2")
                                              .arg(serial, termQuery.lastError().text()));
                    return;
                }
                newTermId = termQuery.value(0).toInt();
            }

            QSqlQuery detailQuery(db);
            detailQuery.prepare("INSERT INTO tblreceiptdetails (receiptdocid, terminalid) VALUES (:did, :tid)");
            detailQuery.bindValue(":did", docId);
            detailQuery.bindValue(":tid", newTermId);
            if (!detailQuery.exec()) {
                db.rollback();
                QMessageBox::critical(this, "Ошибка БД", "Ошибка связи: " + detailQuery.lastError().text());
                return;
            }
        }
    }

    // 4. Фиксируем транзакцию.
    if (!db.commit()) {
        db.rollback();
        QMessageBox::critical(this, "Ошибка", "Не удалось зафиксировать транзакцию");
    } else {
        DatabaseManager::instance().logAction("POST", "tblreceiptdocs", docId);

        QMessageBox::information(this, "Успех", "Документ успешно проведен!");
        DatabaseManager::instance().notifyDataChanged();
        this->close();
    }
}

void ReceiptForm::on_btnPrint_clicked()
{
    if (!m_editMode && ui->lineEditNumber->text().trimmed().isEmpty()) {
        QString num = DatabaseManager::instance().generateDocNumber("receipt");
        if (num.isEmpty()) {
            QMessageBox::critical(this, "Ошибка БД", "Не удалось сгенерировать номер документа.");
            return;
        }
        ui->lineEditNumber->setText(num);
    }

    QString html = "<html><head><meta charset='utf-8'>"
                   "<style>"
                   "body { font-family: 'Times New Roman', serif; font-size: 14px; }"
                   "h2 { text-align: center; }"
                   "table { border-collapse: collapse; width: 100%; margin-top: 20px; }"
                   "th, td { border: 1px solid black; padding: 6px; text-align: left; vertical-align: top; }"
                   "th { background-color: #f0f0f0; }"
                   "</style></head><body>";

    html += "<h2>ПРИХОДНАЯ НАКЛАДНАЯ № " + ui->lineEditNumber->text().toHtmlEscaped() + "</h2>";
    html += "<p>от " + ui->dateEdit->date().toString("dd.MM.yyyy") + " г.</p>";
    html += "<p><b>Поставщик:</b> ООО «POC Terminal»</p>";

    QString comment = ui->textEditComment->toPlainText().trimmed();
    if (!comment.isEmpty())
        html += "<p><b>Комментарий:</b> " + comment.toHtmlEscaped() + "</p>";

    html += "<table><tr><th>№</th><th>Модель</th><th>Кол-во</th><th>Серийные номера</th><th>IMEI 1</th><th>IMEI 2</th></tr>";

    for (int i = 0; i < rowsModel->rowCount(); ++i) {
        const QString model = rowsModel->data(rowsModel->index(i, ColModel)).toString();
        const int qty = rowQty(i);

        QStringList cell;
        for (const QString& v : listForRow(i, ColSerial))
            cell << v.toHtmlEscaped();
        const QString serials = cell.isEmpty() ? "&nbsp;" : cell.join("<br>");

        cell.clear();
        for (const QString& v : listForRow(i, ColImei1))
            cell << v.toHtmlEscaped();
        const QString imei1 = cell.isEmpty() ? "&nbsp;" : cell.join("<br>");

        cell.clear();
        for (const QString& v : listForRow(i, ColImei2))
            cell << v.toHtmlEscaped();
        const QString imei2 = cell.isEmpty() ? "&nbsp;" : cell.join("<br>");

        html += "<tr><td>" + QString::number(i + 1) +
                "</td>"
                "<td>" +
                model.toHtmlEscaped() +
                "</td>"
                "<td>" +
                QString::number(qty) +
                "</td>"
                "<td>" +
                serials +
                "</td>"
                "<td>" +
                imei1 +
                "</td>"
                "<td>" +
                imei2 + "</td></tr>";
    }
    html += "</table>";

    html += "<p style='margin-top: 40px;'>Принял: ________________ / ____________</p>";
    html += "</body></html>";

    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog printDialog(&printer, this);
    if (printDialog.exec() == QDialog::Accepted) {
        QTextDocument doc;
        doc.setHtml(html);
        doc.print(&printer);
    }
}

void ReceiptForm::on_btnClose_clicked()
{
    close();
}

bool ReceiptForm::canAcceptScan() const
{
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
    const BarcodeScan data = BarcodeParser::parse(raw);
    if (!data.hasData())
        return;

    int row = ui->tableView->currentIndex().row();
    if (row < 0) {
        row = rowsModel->rowCount();
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
    }

    if (!data.serial.isEmpty()) {
        QStringList lst = listForRow(row, ColSerial);
        lst.append(data.serial);
        setListForRow(row, ColSerial, lst);
    }
    if (!data.imei1.isEmpty()) {
        QStringList lst = listForRow(row, ColImei1);
        lst.append(data.imei1);
        setListForRow(row, ColImei1, lst);
    }
    if (!data.imei2.isEmpty()) {
        QStringList lst = listForRow(row, ColImei2);
        lst.append(data.imei2);
        setListForRow(row, ColImei2, lst);
    }

    refreshRow(row);
    ui->tableView->setCurrentIndex(rowsModel->index(row, ColSerial));
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