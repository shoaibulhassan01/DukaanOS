#include "user_repository.h"

#include <QCryptographicHash>
#include <QFile>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QTextStream>

#include <limits>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#endif

namespace Pos
{
namespace
{
constexpr int passwordHashIterations = 600000;
constexpr auto passwordHashPrefix = "pbkdf2_sha256$";

bool isValidCredential(const QString &value)
{
    if (value.isEmpty() || value == QStringLiteral("-")
        || value.contains(QLatin1Char('#')))
    {
        return false;
    }
    for (const QChar ch : value)
    {
        if (ch.isSpace())
            return false;
    }
    return true;
}

QByteArray hmacSha256(QCryptographicHash &hash, const QByteArray &innerKey,
    const QByteArray &outerKey, const QByteArray &message)
{
    hash.reset();
    hash.addData(innerKey);
    hash.addData(message);
    const QByteArray innerHash = hash.result();

    hash.reset();
    hash.addData(outerKey);
    hash.addData(innerHash);
    return hash.result();
}

bool pbkdf2Sha256(const QByteArray &password, const QByteArray &salt, int iterations,
    QByteArray &result, QString &error)
{
#ifdef Q_OS_WIN
    if (quint64(password.size()) > std::numeric_limits<ULONG>::max())
    {
        error = QStringLiteral("Password is too long to hash.");
        return false;
    }

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    NTSTATUS status = BCryptOpenAlgorithmProvider(
        &algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (status < 0)
    {
        error = QStringLiteral("The system password hashing service is unavailable (code %1).")
            .arg(QString::number(quint32(status), 16));
        return false;
    }

    QByteArray derived(32, Qt::Uninitialized);
    status = BCryptDeriveKeyPBKDF2(algorithm,
        reinterpret_cast<PUCHAR>(const_cast<char *>(password.constData())),
        ULONG(password.size()),
        reinterpret_cast<PUCHAR>(const_cast<char *>(salt.constData())),
        ULONG(salt.size()), ULONGLONG(iterations),
        reinterpret_cast<PUCHAR>(derived.data()), ULONG(derived.size()), 0);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (status < 0)
    {
        error = QStringLiteral("Password hashing failed (code %1).")
            .arg(QString::number(quint32(status), 16));
        return false;
    }
    result = derived;
    error.clear();
    return true;
#else
    QByteArray normalizedKey = password;
    if (normalizedKey.size() > 64)
        normalizedKey = QCryptographicHash::hash(normalizedKey, QCryptographicHash::Sha256);

    QByteArray innerKey(64, char(0x36));
    QByteArray outerKey(64, char(0x5c));
    for (qsizetype i = 0; i < normalizedKey.size(); ++i)
    {
        innerKey[i] = char(quint8(innerKey.at(i)) ^ quint8(normalizedKey.at(i)));
        outerKey[i] = char(quint8(outerKey.at(i)) ^ quint8(normalizedKey.at(i)));
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);

    QByteArray blockInput = salt;
    blockInput.append("\0\0\0\1", 4);
    QByteArray digest = hmacSha256(hash, innerKey, outerKey, blockInput);
    QByteArray derived = digest;
    for (int i = 1; i < iterations; ++i)
    {
        digest = hmacSha256(hash, innerKey, outerKey, digest);
        for (qsizetype byte = 0; byte < derived.size(); ++byte)
            derived[byte] = char(quint8(derived.at(byte)) ^ quint8(digest.at(byte)));
    }
    result = derived;
    error.clear();
    return true;
#endif
}

bool makePasswordHash(const QString &password, QString &hash, QString &error)
{
    QByteArray salt;
    salt.reserve(16);
    for (int i = 0; i < 4; ++i)
    {
        const quint32 value = QRandomGenerator::system()->generate();
        salt.append(char(value >> 24));
        salt.append(char(value >> 16));
        salt.append(char(value >> 8));
        salt.append(char(value));
    }

    QByteArray derived;
    if (!pbkdf2Sha256(password.toUtf8(), salt, passwordHashIterations, derived, error))
        return false;

    hash = QString::fromLatin1(passwordHashPrefix)
        + QString::number(passwordHashIterations) + QLatin1Char('$')
        + QString::fromLatin1(salt.toHex()) + QLatin1Char('$')
        + QString::fromLatin1(derived.toHex());
    return true;
}

bool constantTimeEquals(const QByteArray &left, const QByteArray &right)
{
    if (left.size() != right.size())
        return false;
    quint8 difference = 0;
    for (qsizetype i = 0; i < left.size(); ++i)
        difference |= quint8(left.at(i)) ^ quint8(right.at(i));
    return difference == 0;
}

bool isPasswordHash(const QString &value)
{
    return value.startsWith(QString::fromLatin1(passwordHashPrefix));
}

bool saveUsers(const QString &filePath, const QVector<User> &users, QString &error)
{
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        error = QStringLiteral("Cannot update %1: %2")
            .arg(filePath, file.errorString());
        return false;
    }

    QTextStream output(&file);
    for (const User &user : users)
        output << user.username << ' ' << user.password << ' ' << user.role << "#\n";
    output.flush();
    if (output.status() != QTextStream::Ok || !file.commit())
    {
        error = QStringLiteral("Could not save %1: %2")
            .arg(filePath, file.errorString());
        return false;
    }
    error.clear();
    return true;
}
}

// Owner: Umar Khan - read user records from Users.txt (shared with Shoaib's login feature).
bool readUsers(QString filePath, QVector<User> &users, QString &error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        error = QStringLiteral("Cannot open %1: %2").arg(filePath, file.errorString());
        return false;
    }

    QVector<User> loadedUsers;
    QTextStream input(&file);
    int lineNumber = 0;
    while (!input.atEnd())
    {
        const QString line = input.readLine().trimmed();
        ++lineNumber;
        if (line.isEmpty())
            continue;
        if (!line.endsWith(QLatin1Char('#')))
        {
            error = QStringLiteral("Users.txt line %1 is missing the ending #.")
                .arg(lineNumber);
            return false;
        }

        const QStringList fields = line.left(line.size() - 1)
            .split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (fields.size() != 3 || fields.at(0) == QStringLiteral("-") ||
            fields.at(1) == QStringLiteral("-") || fields.at(2) == QStringLiteral("-"))
        {
            error = QStringLiteral("Users.txt line %1 must contain username, password and role.")
                .arg(lineNumber);
            return false;
        }

        User user;
        user.username = fields.at(0);
        user.password = fields.at(1);
        user.role = fields.at(2);
        loadedUsers.append(user);
    }

    users = loadedUsers;
    error.clear();
    return true;
}

// Owner: Umar Khan - find a user by exact username.
bool findUser(QVector<User> users, QString username, User &user)
{
    username = username.trimmed();
    for (const User &candidate : users)
    {
        if (candidate.username == username)
        {
            user = candidate;
            return true;
        }
    }
    return false;
}

bool verifyPassword(QString storedPassword, QString password, QString *error)
{
    if (error)
        error->clear();
    if (!isPasswordHash(storedPassword))
        return storedPassword == password;

    const QStringList fields = storedPassword.split(QLatin1Char('$'), Qt::KeepEmptyParts);
    if (fields.size() != 4 || fields.at(0) != QStringLiteral("pbkdf2_sha256"))
        return false;

    bool iterationsValid = false;
    const int iterations = fields.at(1).toInt(&iterationsValid);
    const QByteArray salt = QByteArray::fromHex(fields.at(2).toLatin1());
    const QByteArray expected = QByteArray::fromHex(fields.at(3).toLatin1());
    if (!iterationsValid || iterations < 100000 || iterations > 2000000
        || salt.size() != 16 || expected.size() != 32)
    {
        return false;
    }

    QByteArray actual;
    QString deriveError;
    if (!pbkdf2Sha256(password.toUtf8(), salt, iterations, actual, deriveError))
    {
        if (error)
            *error = deriveError;
        return false;
    }
    return constantTimeEquals(actual, expected);
}

bool updateUserPassword(QString filePath, QString username, QString password,
    QString &error)
{
    QVector<User> users;
    if (!readUsers(filePath, users, error))
        return false;

    bool updated = false;
    for (User &user : users)
    {
        if (user.username == username)
        {
            if (!makePasswordHash(password, user.password, error))
                return false;
            updated = true;
            break;
        }
    }
    if (!updated)
    {
        error = QStringLiteral("Cannot update password: user '%1' was not found.")
            .arg(username);
        return false;
    }
    return saveUsers(filePath, users, error);
}

bool createCashierAccount(QString filePath, QString username, QString password,
    QString &error)
{
    username = username.trimmed();
    if (!isValidCredential(username) || !isValidCredential(password))
    {
        error = QStringLiteral(
            "Username and password must be non-empty and cannot contain spaces or #.");
        return false;
    }

    QVector<User> users;
    if (!readUsers(filePath, users, error))
        return false;
    for (const User &user : users)
    {
        if (user.username == username)
        {
            error = QStringLiteral("That username already exists.");
            return false;
        }
    }

    for (User &user : users)
    {
        if (!isPasswordHash(user.password))
        {
            QString hashedPassword;
            if (!makePasswordHash(user.password, hashedPassword, error))
                return false;
            user.password = hashedPassword;
        }
    }
    QString hashedPassword;
    if (!makePasswordHash(password, hashedPassword, error))
        return false;
    users.append({username, hashedPassword, QStringLiteral("cashier")});
    return saveUsers(filePath, users, error);
}
}
