#include "receiptform.h"
#include "ui_receiptform.h"
#include "database/databasemanager.h"
#include "database/repositories/documentrepository.h"
#include "utils/validator.h"
#include "utils/barcodeparser.h"
#include "utils/barcodescanner.h"
#include "utils/serialscanner.h"
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

    // Настройка даты (сегодня)
    ui->dateEdit->setDate(QDate::currentDate());

    // Номер документа генерируется при проведении (не здесь), чтобы не
    // сжигать значения последовательности для отменённых форм.

    // Настройка модели для табличной части: серийник, IMEI 1, IMEI 2.
    // Модель хранится в UserRole+1 элемента серийного номера, выбор — комбобокс comboModel.
    rowsModel = new QStandardItemModel(0, 3, this);
    rowsModel->setHorizontalHeaderLabels({"Серийный номер", "IMEI 1", "IMEI 2"});
    ui->tableView->setModel(rowsModel);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);

    // Загружаем модели для выпадающего списка
    loadModelsToDelegate();

    // F9 для дублирования строки
    ui->tableView->installEventFilter(this);

    // Сканер штрих-кода: перехват ввода по всей форме, заполнение табличной части.
    m_scanner = new BarcodeScanner(this);
    connect(m_scanner, &BarcodeScanner::scanFinished, this, &ReceiptForm::onScanFinished);
    qApp->installEventFilter(this);

    // Сканер штрих-кода через COM-порт (типовые USB-сканеры в режиме RS-232).
    setupSerialScanner();

    // Начальный фокус — на таблице, чтобы сканер сразу заполнял строки,
    // а не «Номер» документа (canAcceptScan не перехватывает ввод в QLineEdit).
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
        ui->comboModel->addItem(query.value(1).toString(), query.value(0).toInt());
    }

    // Модель выбирается один раз комбобоксом и применяется ко всем строкам.
    if (ui->comboModel->count() > 0)
        ui->comboModel->setCurrentIndex(0);
}

void ReceiptForm::loadForEdit(int docId)
{
    m_editMode = true;
    m_editDocId = docId;

    const QSqlDatabase& db = DatabaseManager::instance().getDatabase();
    DocumentRepository documents(db);

    // Load header
    const models::DocumentHeader header = documents.loadHeader(DocumentRepository::Receipt, docId);
    if (header.id != 0) {
        ui->lineEditNumber->setText(header.docNumber);
        ui->lineEditNumber->setReadOnly(true);
        ui->dateEdit->setDate(header.date);
        ui->textEditComment->setText(header.comments);
    }

    // Load details
    const auto rows = documents.loadReceiptRows(docId);
    for (const auto& row : rows) {
        int r = rowsModel->rowCount();
        rowsModel->insertRow(r);

        QStandardItem* serialItem = new QStandardItem(row.serialNumber);
        serialItem->setData(row.terminalId, Qt::UserRole);
        serialItem->setData(row.modelId, Qt::UserRole + 1);
        rowsModel->setItem(r, 0, serialItem);

        rowsModel->setItem(r, 1, new QStandardItem(row.imei1));
        rowsModel->setItem(r, 2, new QStandardItem(row.imei2));
    }

    // Модель в комбобоксе — по первой строке документа.
    if (rowsModel->rowCount() > 0) {
        const int modelId = rowsModel->data(rowsModel->index(0, 0), Qt::UserRole + 1).toInt();
        const int idx = ui->comboModel->findData(modelId);
        if (idx >= 0)
            ui->comboModel->setCurrentIndex(idx);
    }

    setWindowTitle(QString("Редактирование поступления ID %1").arg(docId));
}

void ReceiptForm::on_btnAddRow_clicked()
{
    int row = rowsModel->rowCount();
    rowsModel->insertRow(row);

    // Значения по умолчанию: серийник-заглушка, модель из комбобокса.
    QStandardItem* serialItem = new QStandardItem("SN-...");
    serialItem->setData(selectedModelId(), Qt::UserRole + 1);
    rowsModel->setItem(row, 0, serialItem);

    rowsModel->setItem(row, 1, new QStandardItem("000000000000000"));
    rowsModel->setItem(row, 2, new QStandardItem("000000000000000"));
}

void ReceiptForm::on_btnGenerate_clicked()
{
    const int count = ui->spinCount->value();
    rowsModel->removeRows(0, rowsModel->rowCount());

    for (int i = 0; i < count; ++i) {
        int row = rowsModel->rowCount();
        rowsModel->insertRow(row);
        QStandardItem* serialItem = new QStandardItem();
        serialItem->setData(selectedModelId(), Qt::UserRole + 1);
        rowsModel->setItem(row, 0, serialItem);
        rowsModel->setItem(row, 1, new QStandardItem());
        rowsModel->setItem(row, 2, new QStandardItem());
    }

    ui->tableView->setFocus();
}

void ReceiptForm::on_comboModel_currentIndexChanged(int /*index*/)
{
    applyModelToAllRows();
}

int ReceiptForm::selectedModelId() const
{
    return ui->comboModel->currentData().toInt();
}

void ReceiptForm::applyModelToAllRows()
{
    const int modelId = selectedModelId();
    for (int i = 0; i < rowsModel->rowCount(); ++i) {
        QStandardItem* item = rowsModel->item(i, 0);
        if (item)
            item->setData(modelId, Qt::UserRole + 1);
    }
}

void ReceiptForm::on_btnDeleteRow_clicked()
{
    int row = ui->tableView->currentIndex().row();
    if (row >= 0) {
        rowsModel->removeRow(row);
    }
}

void ReceiptForm::on_btnPost_clicked()
{
    if (rowsModel->rowCount() == 0) {
        QMessageBox::warning(this, "Внимание", "Добавьте хотя бы одну строку!");
        return;
    }

    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    if (!db.transaction()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось начать транзакцию");
        return;
    }

    QSqlQuery query(db);
    int docId;

    // 1. Создаем или обновляем шапку документа
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

    // В режиме редактирования — удаляем старые связи
    if (m_editMode) {
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

    // 2. Обрабатываем строки
    for (int i = 0; i < rowsModel->rowCount(); ++i) {
        int terminalId = rowsModel->data(rowsModel->index(i, 0), Qt::UserRole).toInt();
        QString serial = rowsModel->data(rowsModel->index(i, 0)).toString();
        int modelId = rowsModel->data(rowsModel->index(i, 0), Qt::UserRole + 1).toInt();
        QString imei1 = rowsModel->data(rowsModel->index(i, 1)).toString();
        QString imei2 = rowsModel->data(rowsModel->index(i, 2)).toString();

        // Валидация модели
        if (modelId <= 0) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка", QString("Строка %1: выберите модель из списка.").arg(i + 1));
            return;
        }

        // Валидация серийного номера
        if (!Validator::validateSerialNotEmpty(serial)) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка",
                                  QString("Строка %1: серийный номер должен содержать минимум 3 символа.").arg(i + 1));
            return;
        }

        // Валидация IMEI (очищаем от разделителей, но показываем пользователю итог)
        QRegularExpression digitRe("[^\\d]");
        QString cleanImei1 = imei1;
        QString cleanImei2 = imei2;
        cleanImei1.remove(digitRe);
        cleanImei2.remove(digitRe);

        if (cleanImei1 != imei1 && !imei1.isEmpty()) {
            QMessageBox::information(this, "Форматирование IMEI",
                                     QString("Строка %1: IMEI 1 был очищен от разделителей:\n"
                                             "Было: %2\nСтало: %3")
                                         .arg(i + 1)
                                         .arg(imei1, cleanImei1));
        }
        if (cleanImei2 != imei2 && !imei2.isEmpty()) {
            QMessageBox::information(this, "Форматирование IMEI",
                                     QString("Строка %1: IMEI 2 был очищен от разделителей:\n"
                                             "Было: %2\nСтало: %3")
                                         .arg(i + 1)
                                         .arg(imei2, cleanImei2));
        }

        imei1 = cleanImei1;
        imei2 = cleanImei2;

        if (!imei1.isEmpty() && imei1.length() != 15) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка",
                                  QString("Строка %1: IMEI 1 должен содержать ровно 15 цифр (сейчас: %2, длина: %3)")
                                      .arg(i + 1)
                                      .arg(imei1)
                                      .arg(imei1.length()));
            return;
        }

        if (!imei2.isEmpty() && imei2.length() != 15) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка",
                                  QString("Строка %1: IMEI 2 должен содержать ровно 15 цифр (сейчас: %2, длина: %3)")
                                      .arg(i + 1)
                                      .arg(imei2)
                                      .arg(imei2.length()));
            return;
        }

        int newTermId;
        if (m_editMode && terminalId > 0) {
            // Существующий терминал — UPDATE
            QSqlQuery termQuery(db);
            termQuery.prepare("UPDATE tblterminals SET serialnumber = :sn, modelid = :mid, "
                              "imei1 = :i1, imei2 = :i2 WHERE terminalid = :tid");
            termQuery.bindValue(":sn", serial);
            termQuery.bindValue(":mid", modelId);
            termQuery.bindValue(":i1", imei1);
            termQuery.bindValue(":i2", imei2);
            termQuery.bindValue(":tid", terminalId);

            if (!termQuery.exec()) {
                db.rollback();
                QMessageBox::critical(
                    this, "Ошибка БД",
                    QString("Ошибка при обновлении терминала %1:\n%2").arg(serial, termQuery.lastError().text()));
                return;
            }
            newTermId = terminalId;
        } else {
            // Новый терминал — INSERT
            QSqlQuery termQuery(db);
            termQuery.prepare("INSERT INTO tblterminals (serialnumber, modelid, imei1, imei2, status) "
                              "VALUES (:sn, :mid, :i1, :i2, 0) RETURNING terminalid");
            termQuery.bindValue(":sn", serial);
            termQuery.bindValue(":mid", modelId);
            termQuery.bindValue(":i1", imei1);
            termQuery.bindValue(":i2", imei2);

            if (!termQuery.exec() || !termQuery.next()) {
                db.rollback();
                QMessageBox::critical(
                    this, "Ошибка БД",
                    QString("Ошибка при добавлении терминала %1:\n%2").arg(serial, termQuery.lastError().text()));
                return;
            }
            newTermId = termQuery.value(0).toInt();
        }

        // Вставляем связь с документом
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

    // 3. Фиксируем транзакцию
    if (!db.commit()) {
        db.rollback();
        QMessageBox::critical(this, "Ошибка", "Не удалось зафиксировать транзакцию");
    } else {
        // Логирование действия
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
                   "th, td { border: 1px solid black; padding: 6px; text-align: left; }"
                   "th { background-color: #f0f0f0; }"
                   "</style></head><body>";

    html += "<h2>ПРИХОДНАЯ НАКЛАДНАЯ № " + ui->lineEditNumber->text().toHtmlEscaped() + "</h2>";
    html += "<p>от " + ui->dateEdit->date().toString("dd.MM.yyyy") + " г.</p>";
    html += "<p><b>Поставщик:</b> ООО «POC Terminal»</p>";

    QString comment = ui->textEditComment->toPlainText().trimmed();
    if (!comment.isEmpty())
        html += "<p><b>Комментарий:</b> " + comment.toHtmlEscaped() + "</p>";

    html += "<table><tr><th>№</th><th>Серийный номер</th><th>Модель</th><th>IMEI 1</th><th>IMEI 2</th></tr>";

    for (int i = 0; i < rowsModel->rowCount(); ++i) {
        QString serial = rowsModel->data(rowsModel->index(i, 0)).toString();
        int modelId = rowsModel->data(rowsModel->index(i, 0), Qt::UserRole + 1).toInt();
        QString modelName;
        for (const auto& m : m_models) {
            if (m.first == modelId) {
                modelName = m.second;
                break;
            }
        }
        QString imei1 = rowsModel->data(rowsModel->index(i, 1)).toString();
        QString imei2 = rowsModel->data(rowsModel->index(i, 2)).toString();

        html += "<tr><td>" + QString::number(i + 1) +
                "</td>"
                "<td>" +
                serial.toHtmlEscaped() +
                "</td>"
                "<td>" +
                modelName.toHtmlEscaped() +
                "</td>"
                "<td>" +
                imei1.toHtmlEscaped() +
                "</td>"
                "<td>" +
                imei2.toHtmlEscaped() + "</td></tr>";
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

    // Серийный номер: первая строка без серийника; если все заполнены — новая строка.
    if (!data.serial.isEmpty()) {
        int row = targetRowForSerial();
        if (row < 0) {
            row = rowsModel->rowCount();
            rowsModel->insertRow(row);
        }
        QStandardItem* serialItem = new QStandardItem(data.serial);
        serialItem->setData(selectedModelId(), Qt::UserRole + 1);
        rowsModel->setItem(row, 0, serialItem);
        ui->tableView->setCurrentIndex(rowsModel->index(row, 0));
    }

    // IMEI: в строку, где серийник заполнен, а нужная колонка пуста.
    if (!data.imei1.isEmpty()) {
        int row = targetRowForImei(1);
        if (row < 0) {
            row = rowsModel->rowCount();
            rowsModel->insertRow(row);
            QStandardItem* serialItem = new QStandardItem();
            serialItem->setData(selectedModelId(), Qt::UserRole + 1);
            rowsModel->setItem(row, 0, serialItem);
        }
        rowsModel->setItem(row, 1, new QStandardItem(data.imei1));
        ui->tableView->setCurrentIndex(rowsModel->index(row, 0));
    }

    if (!data.imei2.isEmpty()) {
        int row = targetRowForImei(2);
        if (row < 0) {
            row = rowsModel->rowCount();
            rowsModel->insertRow(row);
            QStandardItem* serialItem = new QStandardItem();
            serialItem->setData(selectedModelId(), Qt::UserRole + 1);
            rowsModel->setItem(row, 0, serialItem);
        }
        rowsModel->setItem(row, 2, new QStandardItem(data.imei2));
        ui->tableView->setCurrentIndex(rowsModel->index(row, 0));
    }

    ui->tableView->setFocus();
}

int ReceiptForm::targetRowForSerial() const
{
    for (int r = 0; r < rowsModel->rowCount(); ++r) {
        if (rowsModel->data(rowsModel->index(r, 0)).toString().isEmpty())
            return r;
    }
    return -1;
}

int ReceiptForm::targetRowForImei(int imeiCol) const
{
    for (int r = 0; r < rowsModel->rowCount(); ++r) {
        const QString serial = rowsModel->data(rowsModel->index(r, 0)).toString();
        const QString imei = rowsModel->data(rowsModel->index(r, imeiCol)).toString();
        if (!serial.isEmpty() && imei.isEmpty())
            return r;
    }
    return -1;
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