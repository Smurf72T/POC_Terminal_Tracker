#ifndef BACKUPMANAGER_INTERNAL_H
#define BACKUPMANAGER_INTERNAL_H

// Общие константы и вспомогательные функции BackupManager, используемые в
// нескольких translation units (backupmanager / fallback / restore / helpers).
// Изолированы от публичного backupmanager.h, чтобы не расширять его API.

#include <QString>
#include <QStringList>

#include <atomic>

class QProcess;

inline constexpr int kPgDumpTimeoutMs = 60000;
inline constexpr int kPsqlTimeoutMs = 120000;
inline constexpr int kKillWaitMs = 5000;

// Маркер зашифрованного бэкапа (первая строка файла).
// Формат: "POCENC1\n" + шифротекст openssl enc -aes-256-cbc -pbkdf2.
inline constexpr char kEncMarker[] = "POCENC1\n";

QString findOpenssl();
bool runOpenssl(const QStringList& args, const QString& passphrase, QString* error, std::atomic<bool>* cancelRequested);

// Ожидает завершения процесса, периодически проверяя флаг отмены.
bool waitForFinishedWithCancel(QProcess& process, int timeoutMs, std::atomic<bool>* cancelRequested);

bool isEncryptedBackup(const QString& path);

// Шифрует plain-файл в файл с маркером POCENC1 + шифротекстом AES-256-CBC.
bool encryptBackupFile(const QString& plainPath, const QString& outPath, const QString& passphrase, QString* error,
                       std::atomic<bool>* cancelRequested);

// Подготавливает SQL-файл для psql: расшифровывает бэкап (если он с маркером)
// или копирует как есть (обратная совместимость с незашифрованными дампами).
bool decryptBackupFile(const QString& inPath, const QString& outPath, const QString& passphrase, QString* error,
                       std::atomic<bool>* cancelRequested);

#endif // BACKUPMANAGER_INTERNAL_H