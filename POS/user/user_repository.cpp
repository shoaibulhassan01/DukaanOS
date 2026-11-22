#include "user_repository.h"

#include <QFile>
#include <QSaveFile>
#include <QTextStream>

namespace Pos
{
namespace
{
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
    output << username << ' ' << password << " cashier#\n";
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
