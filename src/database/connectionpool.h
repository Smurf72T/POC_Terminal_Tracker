#ifndef CONNECTIONPOOL_H
#define CONNECTIONPOOL_H

#include <QSqlDatabase>
#include <QMutex>
#include <QWaitCondition>
#include <QHash>
#include <QList>
#include <functional>

// Простой пул QSqlDatabase-соединений для рабочих потоков.
//
// В Qt соединение QSqlDatabase можно использовать только в том потоке, где оно
// создано, поэтому пул хранит свободные соединения раздельно для каждого потока
// (ключ — id потока) и никогда не передаёт соединение в чужой поток.
//
// Пул снижает нагрузку на PostgreSQL: вместо открытия нового соединения на каждую
// операцию (costly handshake + аутентификация) соединение переиспользуется в том же
// потоке. Типичное применение — длительный worker-поток (например, BackupWorker).
class ConnectionPool {
public:
    // Фабрика создаёт НОВОЕ открытое соединение с уникальным connectionName.
    using ConnectionFactory = std::function<QSqlDatabase()>;

    explicit ConnectionPool(ConnectionFactory factory, int maxPerThread = 4);
    ~ConnectionPool();

    // Возвращает соединение для текущего потока. Если свободного нет и лимит
    // maxPerThread не превышен — создаёт новое через фабрику, иначе ждёт release().
    QSqlDatabase acquire();

    // Возвращает соединение в пул (вызывать из того же потока, что и acquire).
    // db обнуляется; соединение НЕ закрывается — оно переиспользуется.
    void release(QSqlDatabase& db);

    // Количество свободных соединений во всех потоках (для тестов/диагностики).
    int idleCount() const;

    // Закрывает и удаляет все свободные соединения. Пул после этого не используется.
    void clear();

private:
    mutable QMutex m_mutex;
    QWaitCondition m_cond;
    ConnectionFactory m_factory;
    int m_maxPerThread;
    QHash<Qt::HANDLE, QList<QSqlDatabase>> m_idle;
    QHash<Qt::HANDLE, int> m_activeCount;
    bool m_closed = false;
};

#endif // CONNECTIONPOOL_H
