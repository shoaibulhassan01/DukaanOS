#include "user_repository.h"

#include <QCryptographicHash>
#include <QFile>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QTextStream>

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

QByteArray hmacSha256(const QByteArray &key, const QByteArray &message)
{
    QByteArray normalizedKey = key;
    if (normalizedKey.size() > 64)
        normalizedKey = QCryptographicHash::hash(normalizedKey, QCryptographicHash::Sha256);

    QByteArray innerKey(64, char(0x36));
    QByteArray outerKey(64, char(0x5c));
    for (qsizetype i = 0; i < normalizedKey.size(); ++i)
    {
        innerKey[i] = char(quint8(innerKey.at(i)) ^ quint8(normalizedKey.at(i)));
        outerKey[i] = char(quint8(outerKey.at(i)) ^ quint8(normalizedKey.at(i)));
    }

    const QByteArray innerHash = QCryptographicHash::hash(
        innerKey + message, QCryptographicHash::Sha256);
    return QCryptographicHash::hash(outerKey + innerHash, QCryptographicHash::Sha256);
}

QByteArray pbkdf2Sha256(const QByteArray &password, const QByteArray &salt, int iterations)
{
    QByteArray blockInput = salt;
    blockInput.append("\0\0\0\1", 4);
    QByteArray digest = hmacSha256(password, blockInput);
    QByteArray result = digest;
    for (int i = 1; i < iterations; ++i)
    {
        digest = hmacSha256(password, digest);
        for (qsizetype byte = 0; byte < result.size(); ++byte)
            result[byte] = char(quint8(result.at(byte)) ^ quint8(digest.at(byte)));
    }
    return result;
}

QString makePasswordHash(const QString &password)
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

    return QString::fromLatin1(passwordHashPrefix)
        + QString::number(passwordHashIterations) + QLatin1Char('$')
        + QString::fromLatin1(salt.toHex()) + QLatin1Char('$')
        + QString::fromLatin1(pbkdf2Sha256(password.toUtf8(), salt,
              passwordHashIterations).toHex());
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

bool verifyPassword(QString storedPassword, QString password)
{
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

    const QByteArray actual = pbkdf2Sha256(password.toUtf8(), salt, iterations);
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
            user.password = makePasswordHash(password);
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
            user.password = makePasswordHash(user.password);
    }
    users.append({username, makePasswordHash(password), QStringLiteral("cashier")});
    return saveUsers(filePath, users, error);
}
}
