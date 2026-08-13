#include <QTest>
#include <QString>
#include "utils/registration_ratelimiter.h"

// Тесты чистой логики LoginForm: rate-limit саморегистрации (RegistrationRateLimiter).
// Логика вынесена из loginform.cpp в utils/registration_ratelimiter.cpp, чтобы
// покрыть её без GUI-зависимостей и БД.
class TestLoginForm : public QObject {
    Q_OBJECT

private slots:
    void init() { m_limiter.clear(); }

    void limiterAllowsUpToMax()
    {
        const qint64 start = QDateTime::currentMSecsSinceEpoch();
        QVERIFY(m_limiter.isAllowed(start));
        m_limiter.recordSuccess(start);
        QVERIFY(m_limiter.isAllowed(start + 1));
        m_limiter.recordSuccess(start + 1);
        QVERIFY(m_limiter.isAllowed(start + 2));
        m_limiter.recordSuccess(start + 2);
        QCOMPARE(m_limiter.size(), RegistrationRateLimiter::kMaxRegistrations);
    }

    void limiterBlocksAfterMax()
    {
        const qint64 start = QDateTime::currentMSecsSinceEpoch();
        for (int i = 0; i < RegistrationRateLimiter::kMaxRegistrations; ++i) {
            QVERIFY(m_limiter.isAllowed(start));
            m_limiter.recordSuccess(start + 1);
        }
        QString msg;
        QVERIFY2(!m_limiter.isAllowed(start + 2, &msg), "после kMaxRegistrations заявка должна блокироваться");
        QVERIFY(!msg.isEmpty());
    }

    void windowRollsOver()
    {
        const qint64 window = RegistrationRateLimiter::kRateLimitWindowMs;
        for (int i = 0; i < RegistrationRateLimiter::kMaxRegistrations; ++i)
            m_limiter.recordSuccess(1000 + i);

        // Сразу после записей (окно ещё не прошло) — блокировка.
        QVERIFY(!m_limiter.isAllowed(1002));
        // Попытка за окном (старше самого позднего из записанных) — все истекли.
        QVERIFY(m_limiter.isAllowed(1002 + window + 1));
        QCOMPARE(m_limiter.size(), 0);
    }

private:
    RegistrationRateLimiter m_limiter;
};

QTEST_MAIN(TestLoginForm)
#include "test_loginform.moc"