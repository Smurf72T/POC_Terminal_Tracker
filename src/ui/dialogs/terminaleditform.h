#ifndef TERMINALEDITFORM_H
#define TERMINALEDITFORM_H

#include <QDialog>

#include <QList>
#include <QPair>

namespace Ui {
class TerminalEditForm;
}

// Диалог редактирования терминала (справочник терминалов, двойной клик по строке).
// Открывается по terminalId, загружает данные через TerminalRepository,
// сохраняет изменения методом update().
class TerminalEditForm : public QDialog {
    Q_OBJECT

public:
    explicit TerminalEditForm(int terminalId, QWidget* parent = nullptr);
    ~TerminalEditForm();

private slots:
    void on_btnSave_clicked();
    void on_btnCancel_clicked();
    void on_checkBoxNoDate_toggled(bool checked);

private:
    void loadModel();
    bool validate();
    bool save();

    Ui::TerminalEditForm* ui;
    int m_terminalId = 0;
    // Пары (modelId, отображаемое имя) для комбобокса моделей.
    QList<QPair<int, QString>> m_models;
};

#endif // TERMINALEDITFORM_H