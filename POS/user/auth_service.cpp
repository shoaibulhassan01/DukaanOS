#include "auth_service.h"

#include "user_repository.h"

namespace Pos
{
// Owner: Shoaib Ul Hassan - authenticate credentials and return the user's role.
bool login(QString filePath, QString username, QString password,
    User &loggedInUser, QString &error)
{
    username = username.trimmed();
    if (username.isEmpty() || password.isEmpty())
    {
        error = QStringLiteral("Enter your username and password.");
        return false;
    }

    QVector<User> users;
    if (!readUsers(filePath, users, error))
        return false;

    User matchedUser;
    if (!findUser(users, username, matchedUser))
    {
        error = QStringLiteral("Invalid username or password.");
        return false;
    }
    if (!verifyPassword(matchedUser.password, password, &error))
    {
        if (error.isEmpty())
            error = QStringLiteral("Invalid username or password.");
        return false;
    }

    if (!matchedUser.password.startsWith(QStringLiteral("pbkdf2_sha256$")))
    {
        if (!updateUserPassword(filePath, username, password, error))
            return false;
        matchedUser.password.clear();
    }

    if (matchedUser.role != QStringLiteral("manager") &&
        matchedUser.role != QStringLiteral("cashier"))
    {
        error = QStringLiteral("This account has an unsupported role.");
        return false;
    }

    matchedUser.password.clear();
    loggedInUser = matchedUser;
    error.clear();
    return true;
}
}
