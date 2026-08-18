#ifndef SERIALSCANNER_H
#define SERIALSCANNER_H

#include <QObject>
#include <QString>
#include <QThread>
#include <atomic>

// Чтение клавиатурного сканера из COM-порта (Windows, WinAPI).
// Сканер посылает ASCII + терминатор CR (0x0D). Данные читаются в отдельном
// потоке; по каждому целому «скану» (строка, завершённая CR/LF) испускается
// scanFinished(raw) — сигнал в поток UI (Qt::QueuedConnection).
class SerialScanner : public QObject {
    Q_OBJECT

public:
    explicit SerialScanner(QObject* parent = nullptr);
    ~SerialScanner() override;

    // Открывает COM-порт и запускает поток чтения. Возвращает false, если
    // порт открыть не удалось. Параметры: 8 бит, без чётности, 1 стоп-бит.
    bool start(const QString& portName, int baudRate);
    void stop();

    bool isRunning() const { return m_thread != nullptr; }

signals:
    void scanFinished(const QString& raw);

private:
    static void readLoop(SerialScanner* self, void* hPort, std::atomic<bool>* started);
    static QString bytesToAscii(const QByteArray& data);

    QThread* m_thread = nullptr;
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_started{false};
    void* m_portHandle = nullptr;
};

#endif // SERIALSCANNER_H