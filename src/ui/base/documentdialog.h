#ifndef DOCUMENTDIALOG_H
#define DOCUMENTDIALOG_H

#include <QDialog>
#include <QStandardItemModel>
#include <QSqlDatabase>

class QLineEdit;
class QDateEdit;
class QTextEdit;
class QTableView;
class TransactionGuard;

namespace models {
struct DocumentHeader;
}

// Базовый диалог документов (поступление/аренда/возврат/изменение статуса/оплата).
// Инкапсулирует общую логику: редактирование, проведение в транзакции, номер, закрытие.
class DocumentDialog : public QDialog {
    Q_OBJECT

public:
    explicit DocumentDialog(QWidget* parent = nullptr);
    ~DocumentDialog() override;

    void loadForEdit(int docId);

    // Общий сценарий проведения: валидация → транзакция → шапка → детали → commit → успех.
    void executePost();

protected:
    // --- интерфейс для потомков ---
    virtual QString docType() const = 0;
    virtual QLineEdit* headerNumberEdit() const = 0;
    virtual QDateEdit* headerDateEdit() const = 0;
    virtual QTextEdit* headerCommentEdit() const = 0;
    virtual QTableView* tableView() const = 0;

    virtual bool validateBeforePost() = 0;
    virtual int postHeader(QSqlDatabase& db) = 0;
    virtual bool postDetails(QSqlDatabase& db, int docId) = 0;
    virtual void onPostSuccess(int docId) = 0;
    virtual void loadSpecificEditData(int docId) = 0;

    // Генерация номера, если поле пустое. true — номер установлен.
    bool ensureDocNumber();

    QStandardItemModel* rowsModel = nullptr;
    bool m_editMode = false;
    int m_editDocId = 0;
};

#endif // DOCUMENTDIALOG_H
