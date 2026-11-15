#include "user_repository.h"

namespace Pos
{
// TODO: Read user records from Users.txt.
bool readUsers(QString filePath, QVector<User> &users, QString &error)
{
    error = QStringLiteral("TODO: Read users from Users.txt.");
    return false;
}

// TODO: Find a user by username.
bool findUser(QVector<User> users, QString username, User &user)
{
    return false;
}
}
