#include "season_service.h"

#include "season_repository.h"
#include "../common/data_paths.h"

#include <cmath>
#include <QRegularExpression>

namespace Pos
{
namespace
{
bool validSeason(const Season &season, QString &error)
{
    if (season.name.trimmed().isEmpty() || season.name != season.name.trimmed()
        || season.name.contains(QLatin1Char('#')) || season.name.contains(QRegularExpression(QStringLiteral("\\s"))))
        error = QStringLiteral("Season name must be a single word.");
    else if (!season.startDate.isValid() || !season.endDate.isValid())
        error = QStringLiteral("Season dates are invalid.");
    else if (season.startDate > season.endDate)
        error = QStringLiteral("Start date cannot be after end date.");
    else if (!std::isfinite(season.percentage) || season.percentage <= 0
        || season.percentage > 100)
        error = QStringLiteral("Percentage must be between 0 and 100.");
    else
    {
        error.clear();
        return true;
    }
    return false;
}

bool overlaps(const Season &season, const QVector<Season> &seasons, int excluded = -1)
{
    for (int i = 0; i < seasons.size(); ++i)
    {
        if (i == excluded)
            continue;
        if (season.startDate <= seasons.at(i).endDate
            && season.endDate >= seasons.at(i).startDate)
            return true;
    }
    return false;
}

bool load(QVector<Season> &seasons, QString &error)
{
    return readSeasons(DataPaths::seasons(), seasons, error);
}

bool store(const QVector<Season> &seasons, QString &error)
{
    return saveSeasons(DataPaths::seasons(), seasons, error);
}
}

bool addSeason(const Season &season, QString &error)
{
    if (!validSeason(season, error))
        return false;
    QVector<Season> seasons;
    if (!load(seasons, error))
        return false;
    if (overlaps(season, seasons))
    {
        error = QStringLiteral("Season dates overlap with an existing season.");
        return false;
    }
    seasons.append(season);
    return store(seasons, error);
}

bool updateSeason(int index, const Season &season, QString &error)
{
    if (!validSeason(season, error))
        return false;
    QVector<Season> seasons;
    if (!load(seasons, error))
        return false;
    if (index < 0 || index >= seasons.size())
    {
        error = QStringLiteral("Season not found.");
        return false;
    }
    if (overlaps(season, seasons, index))
    {
        error = QStringLiteral("Season dates overlap with another season.");
        return false;
    }
    seasons[index] = season;
    return store(seasons, error);
}

bool setSeasonActive(int index, bool isActive, QString &error)
{
    QVector<Season> seasons;
    if (!load(seasons, error))
        return false;
    if (index < 0 || index >= seasons.size())
    {
        error = QStringLiteral("Season not found.");
        return false;
    }
    seasons[index].isActive = isActive;
    return store(seasons, error);
}

bool findActiveSeason(QDate date, Season &season, QString &error)
{
    error.clear();
    if (!date.isValid())
    {
        error = QStringLiteral("Invalid date.");
        return false;
    }
    QVector<Season> seasons;
    if (!load(seasons, error))
        return false;
    for (const Season &candidate : seasons)
    {
        if (candidate.isActive && date >= candidate.startDate && date <= candidate.endDate)
        {
            season = candidate;
            return true;
        }
    }
    error.clear();
    return false;
}

bool getAllSeasons(QVector<Season> &seasons, QString &error)
{
    return load(seasons, error);
}

bool deleteSeason(int index, QString &error)
{
    QVector<Season> seasons;
    if (!load(seasons, error))
        return false;
    if (index < 0 || index >= seasons.size())
    {
        error = QStringLiteral("Season not found.");
        return false;
    }
    seasons.removeAt(index);
    return store(seasons, error);
}
}
