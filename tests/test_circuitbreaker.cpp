#include <QtTest>
#include "utils/circuitbreaker.h"

class TestCircuitBreaker : public QObject {
    Q_OBJECT

private slots:
    void startsClosed();
    void opensAfterThreshold();
    void blocksWhileOpen();
    void resetsOnSuccess();
    void halfOpenAllowsTrialAndReopensOnFailure();
    void halfOpenClosesOnSuccess();
};

void TestCircuitBreaker::startsClosed()
{
    CircuitBreaker cb;
    QCOMPARE(cb.state(), CircuitBreaker::State::Closed);
    QVERIFY(cb.isAllowed());
}

void TestCircuitBreaker::opensAfterThreshold()
{
    CircuitBreaker cb(3, 60000);
    for (int i = 0; i < 3; ++i)
        cb.onFailure();
    QCOMPARE(cb.state(), CircuitBreaker::State::Open);
    QVERIFY(!cb.isAllowed());
}

void TestCircuitBreaker::blocksWhileOpen()
{
    CircuitBreaker cb(2, 60000);
    cb.onFailure();
    cb.onFailure();
    QVERIFY(!cb.isAllowed());
    QVERIFY(!cb.isAllowed());
}

void TestCircuitBreaker::resetsOnSuccess()
{
    CircuitBreaker cb(2, 60000);
    cb.onFailure();
    cb.onSuccess();
    QCOMPARE(cb.state(), CircuitBreaker::State::Closed);
    // После успеха счётчик сброшен: одна неудача снова не открывает breaker
    cb.onFailure();
    QCOMPARE(cb.state(), CircuitBreaker::State::Closed);
    QVERIFY(cb.isAllowed());
}

void TestCircuitBreaker::halfOpenAllowsTrialAndReopensOnFailure()
{
    CircuitBreaker cb(1, 1, 1);
    cb.onFailure(); // threshold 1 → открыт
    QCOMPARE(cb.state(), CircuitBreaker::State::Open);
    QTest::qWait(5);
    QVERIFY(cb.isAllowed()); // cooldown прошёл → half-open, 1 пробный запрос
    cb.onFailure();          // пробный запрос упал → снова открыт
    QCOMPARE(cb.state(), CircuitBreaker::State::Open);
    QVERIFY(!cb.isAllowed());
}

void TestCircuitBreaker::halfOpenClosesOnSuccess()
{
    CircuitBreaker cb(1, 1, 1);
    cb.onFailure();
    QTest::qWait(5);
    QVERIFY(cb.isAllowed());
    cb.onSuccess();
    QCOMPARE(cb.state(), CircuitBreaker::State::Closed);
}

QTEST_MAIN(TestCircuitBreaker)
#include "test_circuitbreaker.moc"
