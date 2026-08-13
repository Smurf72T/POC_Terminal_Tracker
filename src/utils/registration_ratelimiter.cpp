#include "registration_ratelimiter.h"

#include <algorithm>

bool RegistrationRateLimiter::isAllowed(qint64 nowMs, QString* blockMessage)
{
    // Выбрасываем устаревшие попытки за пределами окна.
    m_registerAttempts.erase(std::remove_if(m_registerAttempts.begin(), m_registerAttempts.end(),
                                            [nowMs](qint64 t) { return nowMs - t > kRateLimitWindowMs; }),
                             m_registerAttempts.end());

    if (m_registerAttempts.size() >= kMaxRegistrations) {
        if (blockMessage) {
            const qint64 oldest = m_registerAttempts.first();
            const int minutes = static_cast<int>((kRateLimitWindowMs - (nowMs - oldest)) / 60000) + 1;
            *blockMessage = QString("Слишком много попыток регистрации (максимум %1 за %2 мин). Подождите ~%3 мин.")
                                .arg(kMaxRegistrations)
                                .arg(kRateLimitWindowMs / 60000)
                                .arg(minutes);
        }
        return false;
    }
    return true;
}

void RegistrationRateLimiter::recordSuccess(qint64 nowMs)
{
    m_registerAttempts.append(nowMs);
}