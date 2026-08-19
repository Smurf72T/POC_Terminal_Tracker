#include "backupmanager_internal.h"

#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryFile>

QString findOpenssl()
{
    QString found = QStandardPaths::findExecutable("openssl");
    if (!found.isEmpty())
        return found;
    // Резервные пути (Git for Windows / OpenSSL) — в CI openssl не всегда в PATH.
    const QStringList fallbackPaths = {
#ifdef Q_OS_WIN
        "C:/Program Files/Git/usr/bin/openssl.exe",
        "C:/Program Files/OpenSSL-Win64/bin/openssl.exe",
        "C:/OpenSSL-Win64/bin/openssl.exe",
#endif
        "/usr/bin/openssl",
        "/usr/local/bin/openssl",
    };
    for (const QString& p : fallbackPaths) {
        if (QFileInfo::exists(p))
            return p;
    }
    return QString();
}

bool runOpenssl(const QStringList& args, const QString& passphrase, QString* error, std::atomic<bool>* cancelRequested)
{
    static const QString kOpenssl = findOpenssl();
    if (kOpenssl.isEmpty()) {
        if (error)
            *error = "openssl не найден — невозможно выполнить шифрование/расшифровку бэкапа";
        return false;
    }
    QProcess process;
    process.start(kOpenssl, args);
    if (!process.waitForStarted(kKillWaitMs)) {
        if (error)
            *error = "Не удалось запустить openssl: " + process.errorString();
        return false;
    }
    // Пароль передаём через stdin (аргумент -pass stdin), чтобы он не был виден
    // в списке аргументов процесса (/proc/<pid>/cmdline).
    if (!passphrase.isEmpty()) {
        process.write(passphrase.toUtf8() + "\n");
    }
    process.closeWriteChannel();
    if (!waitForFinishedWithCancel(process, kPgDumpTimeoutMs, cancelRequested)) {
        process.kill();
        process.waitForFinished(kKillWaitMs);
        if (error)
            *error = cancelRequested && cancelRequested->load() ? "Операция отменена пользователем"
                                                                : "openssl не завершился за 60 секунд и был остановлен";
        return false;
    }
    QString stderrText = process.readAllStandardError();
    if (process.exitCode() != 0) {
        if (error)
            *error = QString("openssl завершился с ошибкой (код %1):\n%2")
                         .arg(process.exitCode())
                         .arg(stderrText.left(1000));
        return false;
    }
    return true;
}

bool waitForFinishedWithCancel(QProcess& process, int timeoutMs, std::atomic<bool>* cancelRequested)
{
    if (!cancelRequested)
        return process.waitForFinished(timeoutMs);
    const int kPollIntervalMs = 200;
    int elapsedMs = 0;
    while (elapsedMs < timeoutMs) {
        if (process.waitForFinished(qMin(kPollIntervalMs, timeoutMs - elapsedMs)))
            return true;
        elapsedMs += kPollIntervalMs;
        if (cancelRequested->load())
            return false;
    }
    return false;
}

bool isEncryptedBackup(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    return f.read(qstrlen(kEncMarker)) == QByteArray(kEncMarker);
}

bool encryptBackupFile(const QString& plainPath, const QString& outPath, const QString& passphrase, QString* error,
                       std::atomic<bool>* cancelRequested)
{
    QTemporaryFile cipherFile("enc-XXXXXX.bin");
    if (!cipherFile.open()) {
        if (error)
            *error = "Не удалось создать временный файл для шифрования";
        return false;
    }
    QString cipherPath = cipherFile.fileName();
    cipherFile.close();

    if (!runOpenssl({"enc", "-aes-256-cbc", "-pbkdf2", "-iter", "100000", "-salt", "-pass", "stdin", "-in", plainPath,
                     "-out", cipherPath},
                    passphrase, error, cancelRequested))
        return false;

    QFile in(cipherPath);
    if (!in.open(QIODevice::ReadOnly)) {
        if (error)
            *error = "Не удалось прочитать временный шифротекст";
        return false;
    }
    QFile out(outPath);
    if (out.exists() && !out.remove()) {
        if (error)
            *error = "Не удалось перезаписать файл: " + outPath;
        return false;
    }
    if (!out.open(QIODevice::WriteOnly)) {
        if (error)
            *error = "Не удалось создать файл: " + outPath;
        return false;
    }
    out.write(kEncMarker, qstrlen(kEncMarker));
    QByteArray buf;
    while (!in.atEnd()) {
        buf = in.read(1024 * 1024);
        out.write(buf);
    }
    out.close();
    in.close();
    return true;
}

bool decryptBackupFile(const QString& inPath, const QString& outPath, const QString& passphrase, QString* error,
                       std::atomic<bool>* cancelRequested)
{
    if (!isEncryptedBackup(inPath)) {
        QFile::remove(outPath);
        return QFile::copy(inPath, outPath);
    }
    if (passphrase.isEmpty()) {
        if (error)
            *error = "Файл бэкапа зашифрован, но пароль не предоставлен для расшифровки";
        return false;
    }

    QTemporaryFile bodyFile("dec-XXXXXX.bin");
    if (!bodyFile.open()) {
        if (error)
            *error = "Не удалось создать временный файл для расшифровки";
        return false;
    }
    {
        QFile src(inPath);
        if (!src.open(QIODevice::ReadOnly)) {
            if (error)
                *error = "Не удалось открыть файл бэкапа: " + inPath;
            return false;
        }
        src.read(qstrlen(kEncMarker));
        QByteArray buf;
        while (!src.atEnd()) {
            buf = src.read(1024 * 1024);
            bodyFile.write(buf);
        }
        src.close();
    }
    bodyFile.close();

    QFile::remove(outPath);
    return runOpenssl({"enc", "-d", "-aes-256-cbc", "-pbkdf2", "-iter", "100000", "-pass", "stdin", "-in",
                       bodyFile.fileName(), "-out", outPath},
                      passphrase, error, cancelRequested);
}