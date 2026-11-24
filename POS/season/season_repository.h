
#pragma once

#include "season.h"

#include <QString>
#include <QVector>

namespace Pos
{
bool readSeasons(QString filePath, QVector<Season> &seasons,
    QString &error);

bool saveSeasons(QString filePath, const QVector<Season> &seasons,
    QString &error);
}
