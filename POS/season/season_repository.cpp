#include "season_repository.h"

#include "../common/record_codec.h"

#include <cmath>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>
#include <QRegularExpression>

namespace Pos
{
namespace
{
bool validSeason(const Season &season)
{
    if (season.name.trimmed().isEmpty() || season.name != season.name.trimmed()
        || season.name.contains(QLatin1Char('#')) || season.name.contains(QRegularExpression(QStringLiteral("\\s")))
        || !season.startDate.isValid() || !season.endDate.isValid()
        || season.startDate > season.endDate || !std::isfinite(season.percentage)
        || season.percentage <= 0 || season.percentage > 100)
        return false;
    return true;
}
}

bool readSeasons(QString filePath, QVector<Season> &seasons, QString &error)
{
    seasons.clear();
    error.clear();
    QFile file(filePath);
    if (!file.exists())
        return true;
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        error = QStringLiteral("Cannot open %1: %2").arg(filePath, file.errorString());
        return false;
    }

    QVector<Season> loaded;
    QTextStream in(&file);
    int lineNumber = 0;
    while (!in.atEnd())
    {
        const QString line = in.readLine();
        ++lineNumber;
        if (line.trimmed().isEmpty())
            continue;
        QStringList fields;
        if (!splitRecord(line, fields) || fields.size() != 5)
        {
            error = QStringLiteral("Invalid season record at line %1.").arg(lineNumber);
            return false;
        }
        bool percentageOk = false;
        Season season;
        season.name = fields.at(0);
        season.startDate = QDate::fromString(fields.at(1), Qt::ISODate);
        season.endDate = QDate::fromString(fields.at(2), Qt::ISODate);
        season.percentage = fields.at(3).toDouble(&percentageOk);
        season.isActive = fields.at(4) == QStringLiteral("true");
        if ((fields.at(4) != QStringLiteral("true")
                && fields.at(4) != QStringLiteral("false"))
            || !percentageOk || !validSeason(season))
        {
            error = QStringLiteral("Invalid season information at line %1.").arg(lineNumber);
            return false;
        }
        loaded.append(season);
    }
    seasons = loaded;
    return true;
}

bool saveSeasons(QString filePath, const QVector<Season> &seasons, QString &error)
{
    error.clear();
    const QFileInfo info(filePath);
    if (!QDir().mkpath(info.absolutePath()))
    {
        error = QStringLiteral("Cannot create the season data folder.");
        return false;
    }
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        error = QStringLiteral("Cannot write %1: %2").arg(filePath, file.errorString());
        return false;
    }
    QTextStream out(&file);
    for (const Season &season : seasons)
    {
        if (!validSeason(season))
        {
            error = QStringLiteral("Cannot save an invalid season.");
            file.cancelWriting();
            return false;
        }
        out << makeRecord({season.name,
            season.startDate.toString(Qt::ISODate),
            season.endDate.toString(Qt::ISODate),
            QString::number(season.percentage, 'f', 2),
            season.isActive ? QStringLiteral("true") : QStringLiteral("false")}) << '\n';
    }
    out.flush();
    if (out.status() != QTextStream::Ok || !file.commit())
    {
        error = QStringLiteral("Failed to save %1: %2").arg(filePath, file.errorString());
        return false;
    }
    return true;
}
}
