#include "test_db_integration.h"

#include <QFile>
#include <QSqlError>

QMap<QString, QString> loadEnvFile(const QString& filePath)
{
    QMap<QString, QString> env;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return env;

    while (!file.atEnd()) {
        QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;
        int eq = line.indexOf('=');
        if (eq < 0)
            continue;
        env.insert(line.left(eq).trimmed(), line.mid(eq + 1).trimmed());
    }
    return env;
}

QString envValue(const QMap<QString, QString>& env, const QJsonObject& cfg, const QString& envKey,
                 const QString& cfgKey, const QString& def)
{
    if (qEnvironmentVariableIsSet(envKey.toUtf8().constData()))
        return qEnvironmentVariable(envKey.toUtf8().constData());
    if (env.contains(envKey))
        return env[envKey];
    if (cfg.contains(cfgKey))
        return cfg[cfgKey].toString();
    return def;
}

bool openConnection(QSqlDatabase& db, const QString& dbName, const QMap<QString, QString>& env,
                    const QJsonObject& cfg, QString* err)
{
    db.setHostName(envValue(env, cfg, "POC_DB_HOST", "host", "localhost"));
    db.setPort(envValue(env, cfg, "POC_DB_PORT", "port", "5432").toInt());
    db.setDatabaseName(dbName);
    db.setUserName(envValue(env, cfg, "POC_DB_USER", "username", "postgres"));
    db.setPassword(envValue(env, cfg, "POC_DB_PASSWORD", "password", ""));
    db.setConnectOptions("sslmode=disable");
    if (!db.open()) {
        if (err)
            *err = db.lastError().text();
        return false;
    }
    return true;
}