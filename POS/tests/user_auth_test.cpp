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
    if (!Pos::verifyPassword(
            QStringLiteral("pbkdf2_sha256$100000$000102030405060708090a0b0c0d0e0f$"
                           "a29fea0fed85c5b8610c2e5697ea41b5587139e58a388e0c7b7ced30d4e6d8df"),
            QStringLiteral("password")))
    {
        return fail(QStringLiteral("PBKDF2 verification is incompatible with the standard vector."));
    }

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

    QFile storedUsersFile(usersPath);
    if (!storedUsersFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return fail(QStringLiteral("Could not inspect stored user records."));
    const QByteArray storedUsers = storedUsersFile.readAll();
    storedUsersFile.close();
    if (storedUsers.contains("manager test123 manager#"))
        return fail(QStringLiteral("Successful legacy login did not replace the plaintext password."));

    if (!Pos::readUsers(usersPath, users, error))
        return fail(error);
    if (users.size() != 2)
        return fail(QStringLiteral("Legacy login changed the number of users."));
    if (!Pos::verifyPassword(users.at(0).password, QStringLiteral("test123")))
        return fail(QStringLiteral("Migrated manager password did not verify."));

    if (Pos::login(usersPath, QStringLiteral("manager"), QStringLiteral("wrong"),
            authenticatedUser, error))
        return fail(QStringLiteral("Login accepted an incorrect password."));
    if (error != QStringLiteral("Invalid username or password."))
        return fail(QStringLiteral("Login returned an unexpected credential error."));

    if (Pos::login(usersPath, QString(), QStringLiteral("test123"),
            authenticatedUser, error))
        return fail(QStringLiteral("Login accepted an empty username."));

    if (!Pos::createCashierAccount(usersPath, QStringLiteral("newcashier"),
            QStringLiteral("newpass123"), error))
        return fail(error);
    if (!storedUsersFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return fail(QStringLiteral("Could not inspect saved user records."));
    const QByteArray savedUsers = storedUsersFile.readAll();
    storedUsersFile.close();
    if (savedUsers.contains("cashier cashier123 cashier#")
        || savedUsers.contains("newcashier newpass123 cashier#"))
    {
        return fail(QStringLiteral("Cashier account creation left a plaintext password in storage."));
    }

    if (!Pos::login(usersPath, QStringLiteral("newcashier"),
            QStringLiteral("newpass123"), authenticatedUser, error)
        || authenticatedUser.role != QStringLiteral("cashier"))
    {
        return fail(QStringLiteral("New account did not log in as a cashier."));
    }
    if (!Pos::updateCashierAccount(usersPath, QStringLiteral("newcashier"),
            QStringLiteral("renamedcashier"), QString(), error))
        return fail(error);
    if (Pos::login(usersPath, QStringLiteral("newcashier"),
            QStringLiteral("newpass123"), authenticatedUser, error)
        || !Pos::login(usersPath, QStringLiteral("renamedcashier"),
            QStringLiteral("newpass123"), authenticatedUser, error))
    {
        return fail(QStringLiteral("Changing a cashier username must preserve its password."));
    }
    if (!Pos::updateCashierAccount(usersPath, QStringLiteral("renamedcashier"),
            QStringLiteral("renamedcashier"), QStringLiteral("changedpass123"), error))
        return fail(error);
    if (Pos::login(usersPath, QStringLiteral("renamedcashier"),
            QStringLiteral("newpass123"), authenticatedUser, error)
        || !Pos::login(usersPath, QStringLiteral("renamedcashier"),
            QStringLiteral("changedpass123"), authenticatedUser, error))
    {
        return fail(QStringLiteral("Changing a cashier password must replace the old password."));
    }
    if (Pos::updateCashierAccount(usersPath, QStringLiteral("manager"),
            QStringLiteral("manager_renamed"), QString(), error))
    {
        return fail(QStringLiteral("Cashier account updates must not rename managers."));
    }
    if (Pos::createCashierAccount(usersPath, QStringLiteral("renamedcashier"),
            QStringLiteral("anotherpass"), error))
    {
        return fail(QStringLiteral("User repository accepted a duplicate username."));
    }
    if (Pos::createCashierAccount(usersPath, QStringLiteral("bad name"),
            QStringLiteral("pass123"), error))
    {
        return fail(QStringLiteral("User repository accepted a username containing spaces."));
    }

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
