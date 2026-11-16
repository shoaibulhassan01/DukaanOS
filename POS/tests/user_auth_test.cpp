#include "../user/auth_service.h"
#include "../user/user_repository.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

namespace
{
int fail(const QString &message)
{
    QTextStream(stderr) << message << Qt::endl;
    return 1;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid())
        return fail(QStringLiteral("Could not create temporary test directory."));

    const QString usersPath = directory.filePath(QStringLiteral("Users.txt"));
    QFile usersFile(usersPath);
    if (!usersFile.open(QIODevice::WriteOnly | QIODevice::Text))
        return fail(QStringLiteral("Could not create temporary Users.txt."));
    QTextStream output(&usersFile);
    output << "manager test123 manager#\n"
              "cashier cashier123 cashier#\n";
    usersFile.close();

    QVector<Pos::User> users;
    QString error;
    if (!Pos::readUsers(usersPath, users, error) || users.size() != 2)
        return fail(error.isEmpty() ? QStringLiteral("User repository read an unexpected number of records.")
                                    : error);

    Pos::User authenticatedUser;
    if (!Pos::login(usersPath, QStringLiteral("manager"), QStringLiteral("test123"),
            authenticatedUser, error))
        return fail(error);
    if (authenticatedUser.username != QStringLiteral("manager") ||
        authenticatedUser.role != QStringLiteral("manager"))
        return fail(QStringLiteral("Login did not return the manager account."));

    if (Pos::login(usersPath, QStringLiteral("manager"), QStringLiteral("wrong"),
            authenticatedUser, error))
        return fail(QStringLiteral("Login accepted an incorrect password."));
    if (error != QStringLiteral("Invalid username or password."))
        return fail(QStringLiteral("Login returned an unexpected credential error."));

    if (Pos::login(usersPath, QString(), QStringLiteral("test123"),
            authenticatedUser, error))
        return fail(QStringLiteral("Login accepted an empty username."));

    QFile malformedFile(usersPath);
    if (!malformedFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return fail(QStringLiteral("Could not write malformed test data."));
    QTextStream malformedOutput(&malformedFile);
    malformedOutput << "broken user#\n";
    malformedFile.close();

    if (Pos::readUsers(usersPath, users, error))
        return fail(QStringLiteral("User repository accepted a malformed record."));

    return 0;
}
