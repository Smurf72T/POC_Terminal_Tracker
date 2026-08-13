#ifndef REGISTRATION_RATELIMITER_H
#define REGISTRATION_RATELIMITER_H

#include <QVector>
#include <QString>

// Ограничение саморегистрации на одного клиента: не более kMaxRegistrations
// успешных заявок за kRateLimitWindowMs (в памяти, не переживает перезапуск).
// Чистая логика без GUI/БД — покрыта тестами (test_ui_components / test_loginform).
class RegistrationRateLimiter {
public:
    static constexpr int kMaxRegistrations = 3;
    static constexpr qint64 kRateLimitWindowMs = 10 * 60 * 1000; // 10 минут

    // Разрешена ли новая заявка с учётом попыток за окно (nowMs — текущее время,
    // передаётся извне для тестируемости). Если не разрешена, заполняет
    // blockMessage ожиданием (примерное время в минутах).
    bool isAllowed(qint64 nowMs, QString* blockMessage = nullptr);

    // Фиксирует успешную заявку (вызывается ТОЛЬКО после INSERT).
    void recordSuccess(qint64 nowMs);

    int size() const { return m_registerAttempts.size(); }
    void clear() { m_registerAttempts.clear(); }

private:
    // Метки времени успешных заявок (монотонное время).
    QVector<qint64> m_registerAttempts;
};

#endif // REGISTRATION_RATELIMITER_H