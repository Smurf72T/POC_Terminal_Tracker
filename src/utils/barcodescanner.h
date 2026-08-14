#ifndef BARCODESCANNER_H
#define BARCODESCANNER_H

#include <QObject>
#include <QString>
#include <QTimer>

// Детектор клавиатурного сканера (USB keyboard-wedge).
// Печатный ASCII-ввод, поступающий с интервалом < interCharTimeoutMs, копится
// в буфер; скан завершается по паузе между символами или по терминатору
// (Enter/Tab). Буфер короче minLength скан"ом не считается (защита от
// случайных одиночных нажатий). Работает только с QTimer — без GUI.
class BarcodeScanner : public QObject {
    Q_OBJECT

public:
    explicit BarcodeScanner(QObject* parent = nullptr);

    void setInterCharTimeoutMs(int ms);
    int interCharTimeoutMs() const;

    void setMinLength(int length);
    int minLength() const;

    // Идёт ли в данный момент приём скана (буфер не пуст).
    bool isActive() const;

public slots:
    // Подать печатные символы (из eventFilter формы).
    void feed(const QString& text);
    // Завершить текущий буст терминатором. Возвращает true, если терминатор
    // был «поглощён» как конец полноценного скана (буфер >= minLength).
    bool feedTerminator();

signals:
    void scanStarted();
    void scanFinished(const QString& raw);

private:
    // Завершает буфер; возвращает true, если скан был выдан (raw >= minLength).
    bool commit();

    QTimer m_timer;
    QString m_buffer;
    int m_interCharTimeoutMs = 50;
    int m_minLength = 3;
    bool m_committing = false;
};

#endif // BARCODESCANNER_H