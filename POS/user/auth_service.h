#pragma once

#include "user.h"

namespace Pos
{
// Owner: Shoaib Ul Hassan - authenticate the user and return their cashier/manager role.
bool login(QString filePath, QString username, QString password,
    User &loggedInUser, QString &error);
}
