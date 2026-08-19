#include "databasemanager.h"
#include "utils/logging.h"
#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QMessageBox>
#include <QSqlDriver>
#include <QThread>

DatabaseManager& DatabaseManager::instance()
{
    static DatabaseManager instance;
    return instance;
}

bool DatabaseManager::s_suppressDialogs = false;

void DatabaseManager::setSuppressDialogs(bool suppress)
{
    s_suppressDialogs = suppress;
}

bool DatabaseManager::suppressDialogs()
{
    return s_suppressDialogs;
}

IDatabaseManager& databaseManager()
{
    return DatabaseManager::instance();
}

bool DatabaseManager::initialize(const QString& configPath)
{
    if (m_initialized) {
        return true;
    }

    if (!loadConfig(configPath)) {
        showError("Не удалось загрузить конфигурационный файл");
        return false;
    }

    if (!openConnection()) {
        return false;
    }

    if (!runMigrations()) {
        showError("Не удалось применить миграции базы данных: " + m_database.lastError().text());
        close();
        return false;
    }

    seedAdminAccount();

    listenForDataChanges();

    m_initialized = true;
    return true;
}

bool DatabaseManager::loadConfig(const QString& configPath)
{
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray jsonData = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);

    if (doc.isNull()) {
        return false;
    }

    m_config = doc.object();
    m_configPath = configPath;
    return true;
}

bool DatabaseManager::isConnected() const
{
    return m_database.isOpen();
}

void DatabaseManager::close()
{
    if (m_listening) {
        m_database.driver()->unsubscribeFromNotification(QStringLiteral("poc_data_changed"));
        m_listening = false;
    }
    if (m_database.isOpen()) {
        m_database.close();
    }
    m_initialized = false;
}

const QSqlDatabase& DatabaseManager::getDatabase() const
{
    return m_database;
}

QJsonObject DatabaseManager::configObject() const
{
    return m_config;
}

void DatabaseManager::listenForDataChanges()
{
    if (m_listening || !m_database.isOpen())
        return;

    // Подписка на канал NOTIFY 'poc_data_changed' (триггеры из 007-й миграции).
    // Доставленные уведомления эмитятся в dataChanged() — дашборды всех
    // запущенных экземпляров обновляются при изменениях в любом из них.
    QSqlDriver* driver = m_database.driver();
    if (!driver->hasFeature(QSqlDriver::DriverFeature::EventNotifications))
        return;

    if (!driver->subscribeToNotification(QStringLiteral("poc_data_changed"))) {
        qCWarning(logDB) << "Не удалось подписаться на уведомления БД:" << m_database.lastError().text();
        return;
    }

    connect(driver, &QSqlDriver::notification, this,
            [this](const QString& /*channel*/, QSqlDriver::NotificationSource /*source*/, const QVariant& /*payload*/) {
                emit dataChanged();
            });

    m_listening = true;
}

void DatabaseManager::notifyDataChanged()
{
    emit dataChanged();
}

void DatabaseManager::showError(const QString& message)
{
    if (s_suppressDialogs) {
        qCCritical(logDB) << message;
        return;
    }
    // В рабочем потоке (бэкап, миграции) модальный диалог заблокировал бы и поток,
    // и цикл событий UI — логируем вместо показа.
    if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
        qCCritical(logDB) << message;
        return;
    }
    QWidget* parent = QApplication::activeWindow();
    QMessageBox::critical(parent, "Ошибка базы данных", message);
}
