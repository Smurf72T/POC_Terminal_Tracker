#include "circuitbreaker.h"

CircuitBreaker::CircuitBreaker(int failureThreshold, int cooldownMs, int halfOpenTries)
    : m_failureThreshold(qMax(1, failureThreshold)),
      m_cooldownMs(qMax(1, cooldownMs)),
      m_halfOpenTries(qMax(1, halfOpenTries))
{
}

bool CircuitBreaker::isAllowed()
{
    QMutexLocker lock(&m_mutex);
    if (m_state == State::Closed)
        return true;
    if (m_state == State::Open) {
        if (m_openedAt.msecsTo(QDateTime::currentDateTime()) >= m_cooldownMs) {
            m_state = State::HalfOpen;
            m_halfOpenCount = 0;
            return true;
        }
        return false;
    }
    return m_halfOpenCount < m_halfOpenTries;
}

void CircuitBreaker::onSuccess()
{
    QMutexLocker lock(&m_mutex);
    m_failureCount = 0;
    m_halfOpenCount = 0;
    m_state = State::Closed;
}

void CircuitBreaker::onFailure()
{
    QMutexLocker lock(&m_mutex);
    if (m_state == State::HalfOpen) {
        m_state = State::Open;
        m_openedAt = QDateTime::currentDateTime();
        m_failureCount = 0;
        return;
    }
    ++m_failureCount;
    if (m_failureCount >= m_failureThreshold) {
        m_state = State::Open;
        m_openedAt = QDateTime::currentDateTime();
        m_failureCount = 0;
    }
}

CircuitBreaker::State CircuitBreaker::state() const
{
    QMutexLocker lock(&m_mutex);
    return m_state;
}
