#ifndef BACKUPMANAGER_H
#define BACKUPMANAGER_H

#include <QSqlDatabase>
#include <QString>

class BackupManager
{
public:
    struct BackupResult {
        bool ok = false;
        QString filePath;
        QString method;   // "pg_dump" | "fallback"
        bool encrypted = false; // файл зашифрован AES-256-CBC (openssl)
        QString error;
        qint64 size = 0;
    };

    static BackupResult createBackup(const QSqlDatabase &db, const QString &filePath, const QString &password);
    static bool createFallbackBackup(const QSqlDatabase &db, const QString &filePath, const QString &dbname, QString *error = nullptr);
    static bool restoreDatabase(const QSqlDatabase &db, const QString &filePath, const QString &password, QString *error = nullptr);
};

#endif // BACKUPMANAGER_H
