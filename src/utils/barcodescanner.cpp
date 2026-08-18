#include "utils/barcodescanner.h"

BarcodeScanner::BarcodeScanner(QObject* parent) : QObject(parent)
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(m_interCharTimeoutMs);
    connect(&m_timer, &QTimer::timeout, this, &BarcodeScanner::commit);
}

void BarcodeScanner::setInterCharTimeoutMs(int ms)
{
    m_interCharTimeoutMs = qBound(20, ms, 500);
    if (!m_timer.isActive())
        m_timer.setInterval(m_interCharTimeoutMs);
}

int BarcodeScanner::interCharTimeoutMs() const
{
    return m_interCharTimeoutMs;
}

void BarcodeScanner::setMinLength(int length)
{
    m_minLength = qBound(2, length, 512);
}

int BarcodeScanner::minLength() const
{
    return m_minLength;
}

bool BarcodeScanner::isActive() const
{
    return !m_buffer.isEmpty();
}

void BarcodeScanner::feed(const QString& text)
{
    for (QChar ch : text) {
        // Разделители полей (пробел, GS1 \x1D/\x1E/\x04) сохраняем одиночным
        // пробелом — BarcodeParser по ним раскидывает SN/IMEI1/IMEI2.
        if (ch == QChar(0x1D) || ch == QChar(0x1E) || ch == QChar(0x04) || ch.isSpace()) {
            if (!m_buffer.isEmpty() && !m_buffer.endsWith(' ')) {
                m_buffer.append(' ');
                m_timer.start(m_interCharTimeoutMs);
            }
            continue;
        }
        if (!ch.isPrint())
            continue;
        if (m_buffer.isEmpty())
            emit scanStarted();
        m_buffer.append(ch);
        m_timer.start(m_interCharTimeoutMs);
    }
}

bool BarcodeScanner::feedTerminator()
{
    if (m_buffer.isEmpty())
        return false;
    return commit();
}

bool BarcodeScanner::commit()
{
    if (m_committing)
        return false;
    m_committing = true;
    m_timer.stop();
    const QString raw = m_buffer.trimmed();
    m_buffer.clear();
    m_committing = false;
    if (raw.length() < m_minLength)
        return false;
    emit scanFinished(raw);
    return true;
}