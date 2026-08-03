#include "connectionpool.h"

#include <QThread>

ConnectionPool::ConnectionPool(ConnectionFactory factory, int maxPerThread)
    : m_factory(std::move(factory))
    , m_maxPerThread(maxPerThread > 0 ? maxPerThread : 1)
{
}

ConnectionPool::~ConnectionPool()
{
    clear();
}

QSqlDatabase ConnectionPool::acquire()
{
    QMutexLocker locker(&m_mutex);
    const Qt::HANDLE threadId = QThread::currentThreadId();
    QList<QSqlDatabase> &idle = m_idle[threadId];
    int &active = m_activeCount[threadId];

    while (!m_closed && active >= m_maxPerThread && idle.isEmpty())
        m_cond.wait(&m_mutex);

    if (m_closed)
        return QSqlDatabase();

    if (!idle.isEmpty()) {
        QSqlDatabase db = idle.takeLast();
        active++;
        return db;
    }

    QSqlDatabase db = m_factory();
    if (!db.isValid()) {
        const QString name = db.connectionName();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(name);
        return QSqlDatabase();
    }
    active++;
    return db;
}

void ConnectionPool::release(QSqlDatabase &db)
{
    QMutexLocker locker(&m_mutex);
    const Qt::HANDLE threadId = QThread::currentThreadId();
    if (m_activeCount.value(threadId) > 0)
        m_activeCount[threadId]--;

    if (db.isValid()) {
        m_idle[threadId].append(db);
        m_cond.wakeOne();
    }
    db = QSqlDatabase();
}

int ConnectionPool::idleCount() const
{
    QMutexLocker locker(&m_mutex);
    int count = 0;
    for (auto it = m_idle.constBegin(); it != m_idle.constEnd(); ++it)
        count += it.value().size();
    return count;
}

void ConnectionPool::clear()
{
    QMutexLocker locker(&m_mutex);
    m_closed = true;
    m_cond.wakeAll();

    for (auto it = m_idle.begin(); it != m_idle.end(); ++it) {
        for (QSqlDatabase &db : it.value()) {
            const QString name = db.connectionName();
            db.close();
            db = QSqlDatabase();
            QSqlDatabase::removeDatabase(name);
        }
    }
    m_idle.clear();
    m_activeCount.clear();
}
