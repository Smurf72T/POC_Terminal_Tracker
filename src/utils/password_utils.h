#ifndef PASSWORD_UTILS_H
#define PASSWORD_UTILS_H

#include <QString>
#include <QByteArray>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QDataStream>
#include <QIODevice>
#include <QRandomGenerator>
#include <QDateTime>

constexpr int PBKDF2_ITERATIONS = 100000;
constexpr int SALT_BYTES = 16;
constexpr int KEY_BYTES = 32;

inline QByteArray pbkdf2HmacSha256(const QByteArray& password, const QByteArray& salt, int iterations,
                                   int dkLen = KEY_BYTES)
{
    const int hLen = 32;
    const int blocks = (dkLen + hLen - 1) / hLen;
    QByteArray derivedKey;
    derivedKey.reserve(blocks * hLen);

    for (int block = 1; block <= blocks; ++block) {
        QByteArray u;
        {
            QMessageAuthenticationCode mac(QCryptographicHash::Sha256);
            mac.setKey(password);
            mac.addData(salt);
            QByteArray blockBytes;
            QDataStream stream(&blockBytes, QIODevice::WriteOnly);
            stream.setByteOrder(QDataStream::BigEndian);
            stream << quint32(block);
            mac.addData(blockBytes);
            u = mac.result();
        }
        QByteArray t = u;
        for (int j = 2; j <= iterations; ++j) {
            QMessageAuthenticationCode mac(QCryptographicHash::Sha256);
            mac.setKey(password);
            mac.addData(u);
            u = mac.result();
            for (int k = 0; k < u.size(); ++k)
                t[k] = t[k] ^ u[k];
        }
        derivedKey.append(t);
    }
    return derivedKey.left(dkLen);
}

inline QString generateSalt()
{
    QByteArray salt;
    salt.resize(SALT_BYTES);
    for (int i = 0; i < SALT_BYTES; ++i)
        salt[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    return QString::fromLatin1(salt.toHex());
}

inline QString hashPassword(const QString& password, const QString& salt)
{
    QByteArray dk =
        pbkdf2HmacSha256(password.toUtf8(), QByteArray::fromHex(salt.toLatin1()), PBKDF2_ITERATIONS, KEY_BYTES);
    return QString::number(PBKDF2_ITERATIONS) + ':' + salt + ':' + QString::fromLatin1(dk.toHex());
}

inline QString hashPassword(const QString& password)
{
    return hashPassword(password, generateSalt());
}

inline bool constantTimeEquals(const QByteArray& a, const QByteArray& b)
{
    if (a.size() != b.size())
        return false;
    unsigned char result = 0;
    for (int i = 0; i < a.size(); ++i)
        result |= static_cast<unsigned char>(a.at(i)) ^ static_cast<unsigned char>(b.at(i));
    return result == 0;
}

inline bool checkPassword(const QString& password, const QString& storedHash)
{
    if (storedHash.count(':') == 2) {
        QStringList parts = storedHash.split(':');
        bool ok;
        int iterations = parts[0].toInt(&ok);
        if (!ok)
            return false;
        QString salt = parts[1];
        QString expected = parts[2];
        QString actual = hashPassword(password, salt);
        return constantTimeEquals(actual.section(':', 2, 2).toUtf8(), expected.toUtf8());
    }
    if (storedHash.length() == 80) {
        QString salt = storedHash.left(16);
        QString hash =
            QString(QCryptographicHash::hash((salt + password).toUtf8(), QCryptographicHash::Sha256).toHex());
        return constantTimeEquals(hash.toUtf8(), storedHash.mid(16).toUtf8());
    }
    if (storedHash.length() == 64) {
        QString hash = QString(QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex());
        return constantTimeEquals(hash.toUtf8(), storedHash.toUtf8());
    }
    return false;
}

// Результат проверки сложности пароля.
// Единая точка валидации для входа, смены/сброса пароля и регистрации.
struct PasswordStrengthResult {
    bool ok = false;
    QString error;
};

inline PasswordStrengthResult validatePasswordStrength(const QString& password)
{
    if (password.length() < 8)
        return {false, "Пароль должен быть минимум 8 символов."};
    bool hasUpper = false;
    bool hasDigit = false;
    for (const QChar& c : password) {
        if (c.isUpper())
            hasUpper = true;
        else if (c.isDigit())
            hasDigit = true;
    }
    if (!hasUpper)
        return {false, "Пароль должен содержать хотя бы одну заглавную букву."};
    if (!hasDigit)
        return {false, "Пароль должен содержать хотя бы одну цифру."};
    return {true, QString()};
}

// True, если хеш имеет legacy-формат (SHA-256, 64/80 символов) вместо PBKDF2 «iter:salt:hash».
inline bool isLegacyPasswordHash(const QString& storedHash)
{
    return storedHash.count(':') != 2;
}

#endif
