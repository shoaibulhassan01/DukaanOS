
#include "season_service.h"

#include "season_repository.h"
#include "../common/data_paths.h"

#include <QFileInfo>
#include <QVector>
#include <cmath>

namespace Pos
{
namespace
{
bool validateSeason(const Season &season, QString &error)
{
    if (!season.startDate.isValid()
        || !season.endDate.isValid())
    {
        error = QStringLiteral("Invalid season dates.");
        return false;
    }

    if (season.startDate > season.endDate)
    {
        error = QStringLiteral(
            "Start date cannot be after end date.");
        return false;
    }

    if (!std::isfinite(season.percentage)
        || season.percentage <= 0
        || season.percentage > 100)
    {
        error = QStringLiteral(
            "Percentage must be between 0 and 100.");
        return false;
    }

    error.clear();
    return true;
}

bool hasDateOverlap(const Season &season,
    const QVector<Season> &seasons,
    int excludedIndex = -1)
{
    for (int i = 0; i < seasons.size(); i++)
    {
        if (i == excludedIndex)
            continue;

        const Season &existing = seasons[i];

        if (season.startDate <= existing.endDate
            && season.endDate >= existing.startDate)
        {
            return true;
        }
    }

    return false;
}

bool findActiveSeasonForDate(QDate date, Season &season, bool &found,
    QString &error)
{
    found = false;
    if (!date.isValid())
    {
        error = QStringLiteral("Invalid date.");
        return false;
    }

    QVector<Season> seasons;
    const QString seasonFile = DataPaths::seasons();
    if (!QFileInfo::exists(seasonFile))
    {
        error.clear();
        return true;
    }
    if (!readSeasons(seasonFile, seasons, error))
        return false;

    for (const Season &current : seasons)
    {
        if (current.isActive
            && date >= current.startDate
            && date <= current.endDate)
        {
            season = current;
            found = true;
            break;
        }
    }

    error.clear();
    return true;
}
}

bool addSeason(const Season &season, QString &error)
{
    if (!validateSeason(season, error))
        return false;

    QVector<Season> seasons;

    if (!readSeasons(DataPaths::seasons(), seasons, error))
        return false;

    if (hasDateOverlap(season, seasons))
    {
        error = QStringLiteral(
            "Season dates overlap with an existing season.");
        return false;
    }

    seasons.append(season);

    return saveSeasons(DataPaths::seasons(), seasons, error);
}

bool updateSeason(int index, const Season &season,
    QString &error)
{
    if (!validateSeason(season, error))
        return false;

    QVector<Season> seasons;

    if (!readSeasons(DataPaths::seasons(), seasons, error))
        return false;

    if (index < 0 || index >= seasons.size())
    {
        error = QStringLiteral("Season not found.");
        return false;
    }

    if (hasDateOverlap(season, seasons, index))
    {
        error = QStringLiteral(
            "Season dates overlap with another season.");
        return false;
    }

    seasons[index] = season;

    return saveSeasons(DataPaths::seasons(), seasons, error);
}

bool setSeasonActive(int index, bool isActive,
    QString &error)
{
    QVector<Season> seasons;

    if (!readSeasons(DataPaths::seasons(), seasons, error))
        return false;

    if (index < 0 || index >= seasons.size())
    {
        error = QStringLiteral("Season not found.");
        return false;
    }

    seasons[index].isActive = isActive;

    return saveSeasons(DataPaths::seasons(), seasons, error);
}

bool findActiveSeason(QDate date, Season &season,
    QString &error)
{
    bool found = false;
    Season active;
    if (!findActiveSeasonForDate(date, active, found, error))
        return false;

    if (!found)
    {
        error = QStringLiteral("No active season found.");
        return false;
    }

    season = active;
    return true;
}

bool getActiveSeasonDiscountPercentage(QDate date, double &percentage,
    QString &error)
{
    percentage = 0;
    bool found = false;
    Season active;
    if (!findActiveSeasonForDate(date, active, found, error))
        return false;

    if (found)
        percentage = active.percentage;
    error.clear();
    return true;
}

bool getAllSeasons(QVector<Season> &seasons,
    QString &error)
{
    return readSeasons(DataPaths::seasons(), seasons, error);
}

bool deleteSeason(int index, QString &error)
{
    QVector<Season> seasons;

    if (!readSeasons(DataPaths::seasons(), seasons, error))
        return false;

    if (index < 0 || index >= seasons.size())
    {
        error = QStringLiteral("Season not found.");
        return false;
    }

    seasons.removeAt(index);

    return saveSeasons(DataPaths::seasons(), seasons, error);
}
}
