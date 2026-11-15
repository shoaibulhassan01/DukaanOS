#include "auth_service.h"

namespace Pos
{
// TODO: Check the username and password and return the user's role.
bool login(QString filePath, QString username, QString password,
    User &loggedInUser, QString &error)
{
    error = QStringLiteral("TODO: Implement login.");
    return false;
}
}
