
#include "../season/season_service.h"
#include "../season/season_repository.h"
#include "../common/data_paths.h"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QTemporaryDir tempDir;

    if (!tempDir.isValid())
    {
        qCritical() << "Cannot create temporary directory.";
        return 1;
    }

    QDir::setCurrent(tempDir.path());

    if (!QDir().mkpath("Data"))
    {
        qCritical() << "Cannot create temporary Data folder.";
        return 1;
    }

    QString error;
    QVector<Pos::Season> seasons;

    if (!Pos::saveSeasons(Pos::DataPaths::seasons(),
        seasons, error))
    {
        qCritical() << "Initial setup failed:" << error;
        return 1;
    }

    int passed = 0;
    int failed = 0;

    auto check = [&](bool condition, QString message)
    {
        if (condition)
        {
            qInfo() << "[PASS]" << message;
            passed++;
        }
        else
        {
            qCritical() << "[FAIL]" << message;
            failed++;
        }
    };

    Pos::Season first;
    first.startDate = QDate(2026, 1, 1);
    first.endDate = QDate(2026, 1, 31);
    first.percentage = 10;
    first.isActive = true;

    check(Pos::addSeason(first, error),
        "Add valid season");

    Pos::Season overlapping;
    overlapping.startDate = QDate(2026, 1, 31);
    overlapping.endDate = QDate(2026, 2, 10);
    overlapping.percentage = 15;
    overlapping.isActive = true;

    check(!Pos::addSeason(overlapping, error),
        "Reject overlapping season");

    check(error.contains("overlap", Qt::CaseInsensitive),
        "Correct overlap error message");

    Pos::Season second;
    second.startDate = QDate(2026, 2, 1);
    second.endDate = QDate(2026, 2, 28);
    second.percentage = 20;
    second.isActive = true;

    check(Pos::addSeason(second, error),
        "Add non-overlapping season");

    check(Pos::getAllSeasons(seasons, error)
        && seasons.size() == 2,
        "Retrieve all seasons");

    Pos::Season updated;
    updated.startDate = QDate(2026, 3, 1);
    updated.endDate = QDate(2026, 3, 31);
    updated.percentage = 25;
    updated.isActive = true;

    check(Pos::updateSeason(1, updated, error),
        "Update existing season");

    Pos::Season invalidUpdate;
    invalidUpdate.startDate = QDate(2026, 1, 15);
    invalidUpdate.endDate = QDate(2026, 2, 15);
    invalidUpdate.percentage = 30;
    invalidUpdate.isActive = true;

    check(!Pos::updateSeason(1, invalidUpdate, error),
        "Reject overlapping season update");

    check(Pos::setSeasonActive(1, false, error),
        "Deactivate season");

    Pos::Season found;

    check(Pos::findActiveSeason(
        QDate(2026, 1, 15), found, error)
        && found.percentage == 10,
        "Find active season by date");

    check(!Pos::findActiveSeason(
        QDate(2026, 3, 15), found, error),
        "Reject inactive season in date search");

    check(Pos::deleteSeason(1, error),
        "Delete existing season");

    check(!Pos::deleteSeason(10, error),
        "Reject invalid delete index");

    check(Pos::getAllSeasons(seasons, error)
        && seasons.size() == 1,
        "Verify remaining seasons");

    qInfo() << "\n============================";
    qInfo() << "Tests passed:" << passed;
    qInfo() << "Tests failed:" << failed;
    qInfo() << "============================";

    return failed == 0 ? 0 : 1;
}
