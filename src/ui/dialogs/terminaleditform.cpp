#include "terminaleditform.h"
#include "ui_terminaleditform.h"
#include "database/databasemanager.h"
#include "database/repositories/terminalrepository.h"
#include "utils/terminal_status.h"
#include "utils/validator.h"
#include <QMessageBox>
#include <QPushButton>

TerminalEditForm::TerminalEditForm(int terminalId, QWidget* parent)
    : QDialog(parent), ui(new Ui::TerminalEditForm), m_terminalId(terminalId)
{
    ui->setupUi(this);
    setWindowTitle("Редактирование терминала");

    for (int s = 0; s <= TerminalStatus::kMax; ++s)
        ui->comboStatus->addItem(TerminalStatus::name(s), s);

    connect(ui->checkBoxNoDate, &QCheckBox::toggled, this, &TerminalEditForm::on_checkBoxNoDate_toggled);

    loadModel();
}

TerminalEditForm::~TerminalEditForm()
{
    delete ui;
}

void TerminalEditForm::loadModel()
{
    // Справочник моделей (с производителем) для выбора.
    QSqlQuery modelQuery(DatabaseManager::instance().getDatabase());
    modelQuery.prepare("SELECT m.modelid, "
                       "CASE WHEN mf.manufacturername IS NULL THEN m.modelname "
                       "     ELSE mf.manufacturername || ' — ' || m.modelname END AS displayname "
                       "FROM tblmodels m "
                       "LEFT JOIN tblmanufacturers mf ON m.manufacturerid = mf.manufacturerid "
                       "ORDER BY mf.manufacturername, m.modelname");
    if (!modelQuery.exec()) {
        QMessageBox::critical(this, "Ошибка БД", "Не удалось загрузить модели: " + modelQuery.lastError().text());
        return;
    }
    m_models.clear();
    ui->comboModel->clear();
    while (modelQuery.next()) {
        const int modelId = modelQuery.value(0).toInt();
        const QString display = modelQuery.value(1).toString();
        m_models.append(qMakePair(modelId, display));
        ui->comboModel->addItem(display, modelId);
    }

    const models::Terminal t = TerminalRepository(DatabaseManager::instance().getDatabase()).loadById(m_terminalId);
    if (t.id == 0) {
        QMessageBox::critical(this, "Ошибка", "Терминал не найден.");
        close();
        return;
    }

    ui->lineEditSerial->setText(t.serialNumber);
    ui->lineEditImei1->setText(t.imei1);
    ui->lineEditImei2->setText(t.imei2);

    int modelIdx = ui->comboModel->findData(t.modelId);
    ui->comboModel->setCurrentIndex(modelIdx >= 0 ? modelIdx : 0);

    int statusIdx = ui->comboStatus->findData(t.status);
    ui->comboStatus->setCurrentIndex(statusIdx >= 0 ? statusIdx : 0);

    if (t.purchaseDate.isValid()) {
        ui->dateEditPurchase->setDate(t.purchaseDate);
        ui->checkBoxNoDate->setChecked(false);
    } else {
        ui->checkBoxNoDate->setChecked(true);
    }

    ui->textEditNotes->setPlainText(t.notes);
    ui->checkBoxRepaired->setChecked(t.wasRepaired);
    ui->checkBoxDeactivated->setChecked(t.deactivated);
}

bool TerminalEditForm::validate()
{
    if (!Validator::validateSerialNotEmpty(ui->lineEditSerial->text())) {
        QMessageBox::warning(this, "Ошибка", "Серийный номер должен содержать минимум 3 символа.");
        return false;
    }

    if (!Validator::checkUniqueSerial(ui->lineEditSerial->text(), m_terminalId)) {
        QMessageBox::warning(this, "Ошибка", "Терминал с таким серийным номером уже существует.");
        return false;
    }

    const QString imei1 = ui->lineEditImei1->text().trimmed();
    const QString imei2 = ui->lineEditImei2->text().trimmed();
    for (const QString& imei : {imei1, imei2}) {
        if (!imei.isEmpty() && !Validator::validateIMEI(imei)) {
            QMessageBox::warning(this, "Ошибка", "IMEI должен состоять из 15 цифр.");
            return false;
        }
    }

    if (!imei1.isEmpty() && imei1 != "000000000000000" && Validator::checkDuplicateIMEI(imei1, m_terminalId)) {
        QMessageBox::warning(this, "Ошибка", "IMEI 1 уже используется другим терминалом.");
        return false;
    }
    if (!imei2.isEmpty() && imei2 != "000000000000000" && Validator::checkDuplicateIMEI(imei2, m_terminalId)) {
        QMessageBox::warning(this, "Ошибка", "IMEI 2 уже используется другим терминалом.");
        return false;
    }

    return true;
}

bool TerminalEditForm::save()
{
    if (!validate())
        return false;

    TerminalUpdate data;
    data.serialNumber = ui->lineEditSerial->text();
    data.modelId = ui->comboModel->currentData().toInt();
    data.imei1 = ui->lineEditImei1->text().trimmed();
    data.imei2 = ui->lineEditImei2->text().trimmed();
    data.status = ui->comboStatus->currentData().toInt();
    data.purchaseDate = ui->checkBoxNoDate->isChecked() ? QDate() : ui->dateEditPurchase->date();
    data.notes = ui->textEditNotes->toPlainText();
    data.wasRepaired = ui->checkBoxRepaired->isChecked();
    data.deactivated = ui->checkBoxDeactivated->isChecked();

    if (!TerminalRepository(DatabaseManager::instance().getDatabase()).update(m_terminalId, data)) {
        QMessageBox::critical(this, "Ошибка", "Не удалось сохранить изменения терминала.");
        return false;
    }

    DatabaseManager::instance().logAction("UPDATE", "tblterminals", m_terminalId);
    DatabaseManager::instance().notifyDataChanged();
    return true;
}

void TerminalEditForm::on_btnSave_clicked()
{
    if (save())
        accept();
}

void TerminalEditForm::on_btnCancel_clicked()
{
    reject();
}

void TerminalEditForm::on_checkBoxNoDate_toggled(bool checked)
{
    ui->dateEditPurchase->setEnabled(!checked);
}