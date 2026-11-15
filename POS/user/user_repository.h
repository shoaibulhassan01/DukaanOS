#pragma once

#include "user.h"

#include <QString>
#include <QVector>

namespace Pos
{
// Owner: Umar. TODO: Read user records from Users.txt.
bool readUsers(QString filePath, QVector<User> &users, QString &error);

// Owner: Umar. TODO: Find a user by username.
bool findUser(QVector<User> users, QString username, User &user);
}
