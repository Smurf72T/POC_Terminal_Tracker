#include "bulkimportform.h"
#include "ui_bulkimportform.h"
#include "database/databasemanager.h"
#include "utils/validator.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QSqlError>
#include <QSqlQuery>
#include <QDateTime>
#include <QFile>
#include <QStandardItemModel>
#include <QApplication>
#include <QHeaderView>

#include <xlsxdocument.h>
#include <xlsxworksheet.h>

BulkImportForm::BulkImportForm(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BulkImportForm),
    previewModel(nullptr)
{
    ui->setupUi(this);
    setWindowTitle("Массовое поступление терминалов");
    resize(900, 650);
}

BulkImportForm::~BulkImportForm()
{
    delete ui;
}

void BulkImportForm::on_btnSelectFile_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(this,
        "Выберите файл Excel", "",
        "Excel файлы (*.xlsx)");

    if (filePath.isEmpty()) return;

    selectedFilePath = filePath;
    ui->lblFileInfo->setText(QString("Файл: %1").arg(filePath));
    ui->lblFileInfo->setStyleSheet("QLabel { color: #2ecc71; }");
    loadPreview();
}

void BulkImportForm::on_btnExportTemplate_clicked()
{
    QString defaultName = QString("template_import_%1.xlsx")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd"));

    QString filePath = QFileDialog::getSaveFileName(this,
        "Скачать шаблон", defaultName, "Excel (*.xlsx)");

    if (filePath.isEmpty()) return;

    QXlsx::Document document;

    // Стили заголовков
    QXlsx::Format headerStyle;
    headerStyle.setFontBold(true);
    headerStyle.setFontSize(12);
    headerStyle.setPatternBackgroundColor(QColor("#3498db"));
    headerStyle.setFontColor(Qt::white);
    headerStyle.setHorizontalAlignment(QXlsx::Format::AlignHCenter);

    // Заголовки со стилями
    document.write(1, 1, "Серийный номер", headerStyle);
    document.write(1, 2, "IMEI 1", headerStyle);
    document.write(1, 3, "IMEI 2", headerStyle);
    document.write(1, 4, "Модель", headerStyle);
    document.write(1, 5, "Примечание", headerStyle);

    // Пример
    document.write(2, 1, "SN-EXAMPLE-001");
    document.write(2, 2, "000000000000000");
    document.write(2, 3, "000000000000000");
    document.write(2, 4, "Пример модели");
    document.write(2, 5, "Примечание");

    document.setColumnWidth(1, 20);
    document.setColumnWidth(2, 20);
    document.setColumnWidth(3, 20);
    document.setColumnWidth(4, 20);
    document.setColumnWidth(5, 30);

    if (document.saveAs(filePath)) {
        QMessageBox::information(this, "Успех",
            QString("Шаблон сохранён:\n%1\n\nЗаполните колонки:\n"
                    "1. Серийный номер (обязательно, минимум 3 символа)\n"
                    "2. IMEI 1 (обязательно, 15 цифр)\n"
                    "3. IMEI 2 (15 цифр или пусто)\n"
                    "4. Модель (название из справочника)\n"
                    "5. Примечание (необязательно)\n")
            .arg(filePath));
    } else {
        QMessageBox::critical(this, "Ошибка", "Не удалось создать шаблон.");
    }
}

void BulkImportForm::on_btnImport_clicked()
{
    if (selectedFilePath.isEmpty()) {
        QMessageBox::warning(this, "Внимание", "Сначала выберите файл Excel.");
        return;
    }

    if (!QFile::exists(selectedFilePath)) {
        QMessageBox::critical(this, "Ошибка", "Файл не существует.");
        return;
    }

    importData();
}

void BulkImportForm::on_btnClose_clicked()
{
    close();
}

void BulkImportForm::loadPreview()
{
    ui->lblStatus->setText("Загрузка данных из файла...");
    QApplication::processEvents();

    QXlsx::Document document(selectedFilePath);
    QXlsx::Worksheet *worksheet = document.currentWorksheet();
    if (!worksheet) {
        QMessageBox::critical(this, "Ошибка", "Не удалось прочитать лист документа.");
        return;
    }

    QXlsx::CellRange range = worksheet->dimension();
    int maxRows = range.rowCount();
    int maxCols = range.columnCount();

    if (maxRows < 2) {
        QMessageBox::warning(this, "Ошибка", "Файл слишком короткий (нужна хотя бы строка данных после заголовков).");
        return;
    }

    QStandardItemModel *stdModel = new QStandardItemModel(this);
    stdModel->setHorizontalHeaderItem(0, new QStandardItem("Серийный номер"));
    stdModel->setHorizontalHeaderItem(1, new QStandardItem("IMEI 1"));
    stdModel->setHorizontalHeaderItem(2, new QStandardItem("IMEI 2"));
    stdModel->setHorizontalHeaderItem(3, new QStandardItem("Модель"));
    stdModel->setHorizontalHeaderItem(4, new QStandardItem("Примеч��ние"));

    int previewRows = qMin(maxRows - 1, 50);
    for (int row = 2; row <= 1 + previewRows; row++) {
        QList<QStandardItem*> items;
        for (int col = 1; col <= 5; col++) {
            auto cell = worksheet->cellAt(row, col);
            QString val = cell ? cell->value().toString() : "";
            items.append(new QStandardItem(val));
        }
        stdModel->appendRow(items);
    }

    ui->tableViewPreview->setModel(stdModel);
    ui->tableViewPreview->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableViewPreview->horizontalHeader()->setStretchLastSection(true);
    ui->tableViewPreview->setAlternatingRowColors(true);
    ui->tableViewPreview->setEditTriggers(QAbstractItemView::NoEditTriggers);

    ui->lblStatus->setText(QString("Загружено: %1 строк (показано до 50)").arg(previewRows));
}

bool BulkImportForm::importData()
{
    QAbstractItemModel *model = ui->tableViewPreview->model();
    if (!model || model->rowCount() == 0) {
        QMessageBox::warning(this, "Внимание", "Нет данных для импорта.");
        return false;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(this, "Подтверждение",
        QString("Импортировать %1 терминалов?\n\n"
                "Убедитесь, что все модели указаны корректно.").arg(model->rowCount()),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return false;

    auto &dbMgr = DatabaseManager::instance();
    QSqlDatabase db = dbMgr.getDatabase();

    bool success = db.transaction();
    if (!success) {
        QMessageBox::critical(this, "Ошибка", "Не удалось начать транзакцию.");
        return false;
    }

    int imported = 0;
    int failed = 0;
    QStringList errors;
    QSqlQuery query(db);
    QSqlQuery modelQuery(db);

    // Собираем существующие серийные номера
    QSqlQuery existingQuery(db);
    existingQuery.exec("SELECT serialnumber FROM tblterminals");
    QSet<QString> existingSerials;
    while (existingQuery.next()) {
        existingSerials.insert(existingQuery.value(0).toString());
    }

    for (int row = 0; row < model->rowCount(); row++) {
        QString serial = model->data(model->index(row, 0)).toString();
        QString imei1 = model->data(model->index(row, 1)).toString();
        QString imei2 = model->data(model->index(row, 2)).toString();
        QString modelName = model->data(model->index(row, 3)).toString();
        QString notes = model->data(model->index(row, 4)).toString();

        QStringList rowErrors;

        if (serial.length() < 3) {
            rowErrors.append("Серийный номер слишком короткий");
        }
        if (!Validator::validateIMEI(imei1)) {
            rowErrors.append("IMEI 1 должен содержать ровно 15 цифр");
        }
        if (!imei2.isEmpty() && !Validator::validateIMEI(imei2)) {
            rowErrors.append("IMEI 2 должен содержать ровно 15 цифр");
        }
        if (existingSerials.contains(serial)) {
            rowErrors.append("Терминал с таким серийным номером уже существует");
        }

        int modelId = 0;
        if (!modelName.isEmpty()) {
            modelQuery.prepare("SELECT modelid FROM tblmodels WHERE modelname = :name");
            modelQuery.bindValue(":name", modelName);
            if (!modelQuery.exec() || !modelQuery.next()) {
                rowErrors.append(QString("Модель «%1» не найдена").arg(modelName));
            } else {
                modelId = modelQuery.value(0).toInt();
            }
        }

        if (!rowErrors.isEmpty()) {
            failed++;
            errors.append(QString("Строка %1: %2").arg(row + 2).arg(rowErrors.join("; ")));
            continue;
        }

        query.prepare(
            "INSERT INTO tblterminals (serialnumber, modelid, imei1, imei2, status, notes) "
            "VALUES (:sn, :mid, :imei1, :imei2, 0, :notes) RETURNING terminalid"
        );
        query.bindValue(":sn", serial);
        query.bindValue(":mid", modelId);
        query.bindValue(":imei1", imei1);
        query.bindValue(":imei2", imei2);
        query.bindValue(":notes", notes);

        if (query.exec() && query.next()) {
            int terminalId = query.value(0).toInt();
            imported++;
            existingSerials.insert(serial);

            dbMgr.logAction("ADD", "tblterminals", terminalId,
                "admin", "{}",
                QString("serial=%1, imei1=%2").arg(serial).arg(imei1));
        } else {
            failed++;
            errors.append(QString("Строка %1: %2").arg(row + 2).arg(query.lastError().text()));
        }
    }

    if (failed > 0) {
        db.rollback();
        showImportResult(false, model->rowCount(), imported, failed, errors.join("\n"));
        dbMgr.notifyDataChanged();
        return false;
    }

    if (!db.commit()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось завершить транзакцию.");
        return false;
    }

    showImportResult(true, model->rowCount(), imported, failed, QString());
    dbMgr.notifyDataChanged();
    return true;
}

void BulkImportForm::showImportResult(bool success, int totalRows, int importedRows, int failedRows, const QString &errorMsg)
{
    QString msg;
    if (failedRows == 0 && success) {
        msg = QString("Импорт завершён успешно!\n\n"
                      "Всего строк: %1\n"
                      "Импортировано: %2\n"
                      "Ошибок: 0")
                  .arg(totalRows).arg(importedRows);
        QMessageBox::information(this, "Успех", msg);
    } else {
        msg = QString("Импорт завершён с ошибками.\n\n"
                      "Всего строк: %1\n"
                      "Импортировано: %2\n"
                      "Ошибок: %3")
                  .arg(totalRows).arg(importedRows).arg(failedRows);

        if (!errorMsg.isEmpty()) {
            msg += QString("\n\nОшибки:\n%1").arg(errorMsg.left(1000));
        }

        QMessageBox::warning(this, "Частичный успех", msg);
    }
}
