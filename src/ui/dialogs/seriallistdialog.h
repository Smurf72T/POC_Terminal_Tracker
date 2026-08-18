#ifndef SERIALLISTDIALOG_H
#define SERIALLISTDIALOG_H

#include <QDialog>
#include <QStringList>

namespace Ui {
class SerialListDialog;
}

// Окно ввода списка серийных номеров (или IMEI) для одной строки документа
// поступления: по одному значению на строку, живой счётчик и проверка дублей.
class SerialListDialog : public QDialog {
    Q_OBJECT

public:
    enum Mode { Serial, Imei };

    explicit SerialListDialog(Mode mode, int expectedCount, bool strictCount,
                              const QStringList& values, const QString& title, QWidget* parent = nullptr);
    ~SerialListDialog() override;

    // Нормализованные значения (пустые строки отброшены, IMEI очищены от разделителей).
    QStringList values() const;

private slots:
    void updateStatus();
    void onAccepted();

private:
    QStringList parseLines() const;

    Ui::SerialListDialog* ui;
    Mode m_mode;
    int m_expectedCount;
    bool m_strictCount;
};

#endif // SERIALLISTDIALOG_H