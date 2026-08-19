#include "utils/serialscanner.h"

#include <windows.h>
#include <QByteArray>
#include <QDebug>

namespace {

// \\.\COM8 -> COM8 (для обычных имён портов). WinAPI требует префикс \\.\.
QString winPortName(const QString& port)
{
    QString p = port.trimmed().toUpper();
    if (p.isEmpty())
        return QString();
    if (!p.startsWith(QLatin1String("\\\\.\\")))
        p = "\\\\.\\" + p;
    return p;
}

} // namespace

SerialScanner::SerialScanner(QObject* parent) : QObject(parent)
{
}

SerialScanner::~SerialScanner()
{
    stop();
}

bool SerialScanner::start(const QString& portName, int baudRate)
{
    if (m_thread)
        return true; // уже запущен

    const QString name = winPortName(portName);
    if (name.isEmpty()) {
        qWarning() << "SerialScanner: пустое имя порта";
        return false;
    }

    // FILE_FLAG_OVERLAPPED не нужен: читаем в отдельном потоке с блокирующим ReadFile.
    HANDLE h = CreateFileW(reinterpret_cast<const wchar_t*>(name.utf16()),
                           GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        qWarning() << "SerialScanner: не удалось открыть" << name << "код" << GetLastError();
        return false;
    }

    // Настройка порта: 8N1, без аппаратного/программного контроля потока.
    DCB dcb{};
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(h, &dcb)) {
        qWarning() << "SerialScanner: GetCommState failed" << GetLastError();
        CloseHandle(h);
        return false;
    }
    dcb.BaudRate = static_cast<DWORD>(baudRate);
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    if (!SetCommState(h, &dcb)) {
        qWarning() << "SerialScanner: SetCommState failed" << GetLastError();
        CloseHandle(h);
        return false;
    }

    // Таймауты чтения: накопление по паузе 100 мс.
    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout = 100;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 1000;
    SetCommTimeouts(h, &timeouts);

    // Очищаем входной буфер (данные могли накопиться раньше).
    PurgeComm(h, PURGE_RXCLEAR);

    m_portHandle = h;
    m_stop = false;
    m_started = false;

    // Запускаем рабочий поток. Хэндл захватываем по значению — поток не читает
    // m_portHandle параллельно с основным (порт освобождается только в stop()
    // после wait()).
    m_thread = QThread::create([this, h]() { readLoop(this, h, &m_started); });
    m_thread->setParent(this);
    m_thread->start();
    // Ожидаем, пока поток не начнёт чтение (установит m_started), но без deadlock.
    while (!m_started && m_thread->isRunning())
        QThread::msleep(1);

    qInfo() << "SerialScanner: открыт" << name << "на" << baudRate << "бод";
    return true;
}

void SerialScanner::stop()
{
    QThread* worker = m_thread;
    if (!worker)
        return;
    m_stop = true;
    // Прерываем блокирующее ReadFile — ReadFile вернёт ERROR_OPERATION_ABORTED.
    if (m_portHandle)
        CancelIoEx(m_portHandle, nullptr);
    worker->quit();
    if (!worker->wait(2000)) {
        qWarning() << "SerialScanner: поток не остановился за 2с";
    }
    delete worker;
    m_thread = nullptr;
    if (m_portHandle) {
        CloseHandle(m_portHandle);
        m_portHandle = nullptr;
    }
}

void SerialScanner::readLoop(SerialScanner* self, void* hPort, std::atomic<bool>* started)
{
    HANDLE h = static_cast<HANDLE>(hPort);
    QByteArray buffer;
    char tmp[512];

    *started = true;

    while (!self->m_stop) {
        DWORD readBytes = 0;
        const BOOL ok = ReadFile(h, tmp, sizeof(tmp), &readBytes, nullptr);
        if (!ok) {
            const DWORD err = GetLastError();
            if (err == ERROR_OPERATION_ABORTED || self->m_stop)
                break;
            // Переходим на следующий цикл, не рушимся на временной ошибке.
            continue;
        }
        if (readBytes == 0) {
            QThread::msleep(10);
            continue;
        }

        buffer.append(tmp, static_cast<int>(readBytes));

        // Разбиваем по CR/LF — каждый кусок это один скан.
        int cut = 0;
        for (int i = 0; i < buffer.size(); ++i) {
            const char c = buffer[i];
            if (c == '\r' || c == '\n') {
                const QByteArray chunk = buffer.mid(cut, i - cut).trimmed();
                if (!chunk.isEmpty()) {
                    const QString raw = bytesToAscii(chunk);
                    if (!raw.isEmpty())
                        emit self->scanFinished(raw);
                }
                cut = i + 1;
            }
        }
        if (cut > 0) {
            buffer = cut < buffer.size() ? buffer.mid(cut) : QByteArray();
        }
    }
}

QString SerialScanner::bytesToAscii(const QByteArray& data)
{
    QString out;
    out.reserve(data.size());
    for (char ch : data) {
        const unsigned char uc = static_cast<unsigned char>(ch);
        // ASCII печатные + пробел; всё остальное игнорируем (мусор/не-ASCII).
        if (uc >= 0x20 && uc <= 0x7E)
            out.append(QLatin1Char(static_cast<char>(uc)));
        else if (uc == ' ')
            out.append(QLatin1Char(' '));
    }
    return out;
}