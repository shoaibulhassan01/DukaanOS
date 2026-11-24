
#pragma once

#include "season.h"

#include <QDate>
#include <QString>
#include <QVector>

namespace Pos
{
bool addSeason(const Season &season, QString &error);

bool updateSeason(int index, const Season &season,
    QString &error);

bool setSeasonActive(int index, bool isActive,
    QString &error);

bool findActiveSeason(QDate date, Season &season,
    QString &error);

bool getActiveSeasonDiscountPercentage(QDate date, double &percentage,
    QString &error);

bool getAllSeasons(QVector<Season> &seasons,
    QString &error);

bool deleteSeason(int index, QString &error);
}
