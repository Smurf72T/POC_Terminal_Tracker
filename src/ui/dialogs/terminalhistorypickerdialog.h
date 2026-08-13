#ifndef TERMINALHISTORYPICKERDIALOG_H
#define TERMINALHISTORYPICKERDIALOG_H

#include <QDialog>

class QComboBox;

// Диалог выбора терминала для открытия истории (Ctrl+H): редактируемый
// комбобокс с автокомплитом по вхождению и валидацией выбранного серийника.
class TerminalHistoryPickerDialog : public QDialog {
    Q_OBJECT

public:
    explicit TerminalHistoryPickerDialog(QWidget* parent = nullptr);

    int terminalId() const;
    QString serialNumber() const;

private:
    QString m_serial;
    int m_terminalId = -1;
};

#endif // TERMINALHISTORYPICKERDIALOG_H