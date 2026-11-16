#include "user_repository.h"

#include <QFile>
#include <QTextStream>

namespace Pos
{
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
}
