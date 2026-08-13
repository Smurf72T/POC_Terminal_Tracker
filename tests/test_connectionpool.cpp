#include <QtTest>
#include <QThread>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include "database/connectionpool.h"

namespace {
int g_connectionCounter = 0;

QSqlDatabase makeSqliteConnection()
{
    const QString name = QString("pool_test_conn_%1").arg(++g_connectionCounter);
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", name);
    db.setDatabaseName(":memory:");
    if (!db.open()) {
        qWarning("Не удалось открыть QSQLITE: %s", qPrintable(db.lastError().text()));
    }
    return db;
}
} // namespace

class TestConnectionPool : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void reuseConnectionInSameThread();
    void threadsGetSeparateConnections();
    void idleCountAfterRelease();
    void clearReleasesAll();
};

void TestConnectionPool::initTestCase()
{
    if (!QSqlDatabase::isDriverAvailable("QSQLITE"))
        QSKIP("Драйвер QSQLITE недоступен — тесты пула пропущены");
}

void TestConnectionPool::reuseConnectionInSameThread()
{
    ConnectionPool pool(makeSqliteConnection, 2);

    QSqlDatabase db1 = pool.acquire();
    QVERIFY(db1.isOpen());
    const QString first = db1.connectionName();
    pool.release(db1);

    QSqlDatabase db2 = pool.acquire();
    QVERIFY(db2.isOpen());
    // Повторный acquire в том же потоке переиспользует соединение, а не открывает новое
    QCOMPARE(db2.connectionName(), first);
    pool.release(db2);
}

void TestConnectionPool::threadsGetSeparateConnections()
{
    ConnectionPool pool(makeSqliteConnection, 2);

    QString mainName;
    {
        QSqlDatabase db = pool.acquire();
        mainName = db.connectionName();
        pool.release(db);
    }

    QString otherName;
    QThread thread;
    QObject::connect(&thread, &QThread::started, [&]() {
        QSqlDatabase db = pool.acquire();
        otherName = db.connectionName();
        QVERIFY(db.isOpen());
        QSqlQuery query(db);
        QVERIFY(query.exec("SELECT 1"));
        pool.release(db);
        thread.quit();
    });
    thread.start();
    QVERIFY(thread.wait(10000));

    QVERIFY(!mainName.isEmpty());
    QVERIFY(!otherName.isEmpty());
    // Соединения потоков не должны переиспользоваться друг у друга
    QVERIFY(mainName != otherName);
}

void TestConnectionPool::idleCountAfterRelease()
{
    ConnectionPool pool(makeSqliteConnection, 2);
    QCOMPARE(pool.idleCount(), 0);

    QSqlDatabase db = pool.acquire();
    QCOMPARE(pool.idleCount(), 0);
    pool.release(db);
    QCOMPARE(pool.idleCount(), 1);
}

void TestConnectionPool::clearReleasesAll()
{
    ConnectionPool pool(makeSqliteConnection, 2);
    {
        QSqlDatabase db = pool.acquire();
        pool.release(db);
    }
    QCOMPARE(pool.idleCount(), 1);

    pool.clear();
    QCOMPARE(pool.idleCount(), 0);
}

QTEST_MAIN(TestConnectionPool)
#include "test_connectionpool.moc"
