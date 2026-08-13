#ifndef CIRCUITBREAKER_H
#define CIRCUITBREAKER_H

#include <QDateTime>
#include <QMutex>

// Простой circuit breaker для повторяющихся сбоев внешних зависимостей (БД, сеть).
// Закрыт — запросы выполняются. N подряд идущих сбоев переводят в открытое состояние:
// запросы отклоняются мгновенно (fail fast). После cooldown — полуоткрытое: пропускается
// ограниченное число пробных запросов; успех возвращает в закрытое, сбой — снова в открытое.
class CircuitBreaker {
public:
    explicit CircuitBreaker(int failureThreshold = 5, int cooldownMs = 30000, int halfOpenTries = 1);

    bool isAllowed();
    void onSuccess();
    void onFailure();

    enum class State { Closed, Open, HalfOpen };
    State state() const;

private:
    int m_failureThreshold;
    int m_cooldownMs;
    int m_halfOpenTries;
    mutable QMutex m_mutex;
    int m_failureCount = 0;
    int m_halfOpenCount = 0;
    QDateTime m_openedAt;
    State m_state = State::Closed;
};

#endif // CIRCUITBREAKER_H
