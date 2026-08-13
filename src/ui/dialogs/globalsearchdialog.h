#ifndef GLOBALSEARCHDIALOG_H
#define GLOBALSEARCHDIALOG_H

#include <QDialog>

class QLineEdit;
class QListWidget;

// Модальный диалог глобального поиска (Ctrl+K): ищет по таблицам
// терминалов, клиентов, SIM-карт, моделей и производителей по мере ввода.
// При двойном клике или «Открыть» сообщает выбранный тип и id через сигнал.
class GlobalSearchDialog : public QDialog {
    Q_OBJECT

public:
    explicit GlobalSearchDialog(QWidget* parent = nullptr);

signals:
    void itemActivated(int type, int id);

private slots:
    void performSearch(const QString& text);

private:
    int selectedType() const;
    int selectedId() const;
    void openCurrentItem();

    QLineEdit* m_input = nullptr;
    QListWidget* m_list = nullptr;
};

#endif // GLOBALSEARCHDIALOG_H