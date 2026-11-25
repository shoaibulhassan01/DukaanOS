#pragma once

#include "user.h"

#include <QString>
#include <QVector>

namespace Pos
{
// Owner: Umar Khan - read user records from Users.txt (shared with Shoaib's login feature).
bool readUsers(QString filePath, QVector<User> &users, QString &error);

// Owner: Umar Khan - find a user by exact username.
bool findUser(QVector<User> users, QString username, User &user);

bool verifyPassword(QString storedPassword, QString password, QString *error = nullptr);
bool updateUserPassword(QString filePath, QString username, QString password,
    QString &error);

bool createCashierAccount(QString filePath, QString username, QString password,
    QString &error);
}
