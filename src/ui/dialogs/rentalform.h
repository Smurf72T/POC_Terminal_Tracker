#ifndef RENTALFORM_H
#define RENTALFORM_H

#include <QDialog>
#include <QStandardItemModel>
#include <QModelIndex>
#include <QMap>
#include <QPair>

namespace Ui {
class RentalForm;
}

class QSqlDatabase; // forward declaration (методы принимают ссылку)

class RentalForm : public QDialog {
    Q_OBJECT

public:
    explicit RentalForm(QWidget* parent = nullptr);
    ~RentalForm();

    void loadForEdit(int docId);

private slots:
    void on_btnAddRow_clicked();
    void on_btnDeleteRow_clicked();
    void on_btnPost_clicked();
    void on_btnPrintAct_clicked(); // <-- Добавлено
    void on_btnClose_clicked();
    void onTableViewDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);

private:
    Ui::RentalForm* ui;
    QStandardItemModel* rowsModel;
    bool isPosted = false;
    bool m_editMode = false;
    int m_editDocId = 0;
    // Снимок деталей документа из БД (terminalid -> {sim слота 1, sim слота 2})
    // для корректного определения статусов при редактировании проведённого документа.
    QMap<int, QPair<int, int>> m_originalDetails;

    void loadClientsToDelegate();
    void loadFreeTerminalsToDelegate();
    void loadFreeSIMsToDelegate();

    // Находит в справочнике SIM по введённому в ячейку номеру или создаёт карточку.
    // cellSimId — выбранное значение из делегата (Qt::UserRole), cellSimNumber — текст.
    // Возвращает id SIM (>0), 0 если SIM не указана, -1 при ошибке (сообщение в *error).
    int resolveSimFromCell(QSqlDatabase& db, int cellSimId, const QString& cellSimNumber, QString* error);
    // Проверяет и занимает SIM: статус должен быть 0 (свободна). true — успех.
    bool lockSimCard(QSqlDatabase& db, int simId, const QString& context, QString* error);
    // Освобождает SIM (status = 0).
    bool freeSimCard(QSqlDatabase& db, int simId, const QString& context, QString* error);
};

#endif // RENTALFORM_H