#ifndef SERIALUNITSDIALOG_H
#define SERIALUNITSDIALOG_H

#include <QDialog>
#include <QStringList>

namespace Ui {
class SerialUnitsDialog;
}

// Окно ввода комплектов «серийный номер + его IMEI 1 / IMEI 2» одной строки
// документа поступления. Каждая строка таблицы — ОДИН комплект: номер IMEI 1 и
// IMEI 2 жёстко привязаны к своей строке серийного номера и не могут попасть
// к другому серийнику (связь сохраняется всегда).
class SerialUnitsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SerialUnitsDialog(int expectedCount, const QStringList& serials, const QStringList& imei1,
                               const QStringList& imei2, const QString& title, QWidget* parent = nullptr);
    ~SerialUnitsDialog() override;

    // Списки одинаковой длины (по одному комплекту на элемент), пустые
    // значения — null-QString (привязка к БД = NULL).
    QStringList serials() const;
    QStringList imei1() const;
    QStringList imei2() const;

    // Обработка скана сканера (SN → IMEI 1 → IMEI 2 → следующий SN):
    // серийник открывает новый комплект, голый IMEI дописывается к последнему
    // (сначала в IMEI 1, затем в IMEI 2). Возвращает false, если скан
    // пуст/некорректен и таблица не изменилась.
    bool handleScan(const QString& raw);

private slots:
    void updateStatus();
    void onCellChanged(int row, int col);
    void onAccepted();

private:
    void moveToNextCell(int row, int col);
    // Текст ячейки (пустая строка, если ячейки нет).
    QString cellText(int row, int col) const;
    // Установить текст ячейки, создавая её при необходимости.
    void setCellText(int row, int col, const QString& text);

    bool m_populating = false;

    Ui::SerialUnitsDialog* ui;
    int m_expectedCount;
};

#endif // SERIALUNITSDIALOG_H