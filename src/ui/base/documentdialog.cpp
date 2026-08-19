#include "ui/base/documentdialog.h"
#include "ui/base/transactionguard.h"
#include "database/databasemanager.h"
#include "services/documentnumbergenerator.h"
#include <QLineEdit>
#include <QMessageBox>

DocumentDialog::DocumentDialog(QWidget* parent)
    : QDialog(parent), rowsModel(new QStandardItemModel(this))
{
}

DocumentDialog::~DocumentDialog() = default;

void DocumentDialog::loadForEdit(int docId)
{
    m_editMode = true;
    m_editDocId = docId;
    loadSpecificEditData(docId);
}

void DocumentDialog::executePost()
{
    if (!validateBeforePost())
        return;

    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    TransactionGuard guard(db);

    const int docId = postHeader(db);
    if (docId < 0)
        return;

    if (!postDetails(db, docId))
        return;

    if (!guard.commit())
        return;

    onPostSuccess(docId);
}

bool DocumentDialog::ensureDocNumber()
{
    QLineEdit* number = headerNumberEdit();
    if (!number || !number->text().trimmed().isEmpty())
        return true;

    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    const QString num = DocumentNumberGenerator::generate(docType(), db);
    if (num.isEmpty()) {
        QMessageBox::critical(this, "Ошибка БД", "Не удалось сгенерировать номер документа.");
        return false;
    }
    number->setText(num);
    return true;
}
