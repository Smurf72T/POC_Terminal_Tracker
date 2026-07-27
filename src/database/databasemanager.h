#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QMessageBox>

class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    static DatabaseManager& instance();

    bool initialize(const QString& configPath = "config/config.json");
    bool isConnected() const;
    void close();

    QSqlDatabase& getDatabase();
    QSqlQuery executeQuery(const QString& query, bool showErrorMessage = true);
    bool executeTransaction(const std::function<bool(QSqlDatabase&)>& transactionFunc);

    void notifyDataChanged(); // <-- Добавлено

signals:
    void dataChanged(); // <-- Добавлено

private:
    DatabaseManager() = default;
    ~DatabaseManager() = default;
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    bool loadConfig(const QString& configPath);
    void showError(const QString& message);

    QSqlDatabase m_database;
    QJsonObject m_config;
    bool m_initialized = false;
};

#endif // DATABASEMANAGER_H