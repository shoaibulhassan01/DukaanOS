
#include "season_repository.h"

#include "../common/record_codec.h"
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>
#include <QDir>

#include <cmath>

namespace Pos
{
bool readSeasons(QString filePath, QVector<Season> &seasons,
    QString &error)
{
    error.clear();
    seasons.clear();

    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        error = QStringLiteral("Cannot open Seasons.txt.");
        return false;
    }

    QVector<Season> loaded;
    QTextStream in(&file);

    while (!in.atEnd())
    {
        QString line = in.readLine();

        if (line.trimmed().isEmpty())
            continue;

        QStringList fields;

        if (!splitRecord(line, fields) || fields.size() != 4)
        {
            error = QStringLiteral("Invalid season record.");
            return false;
        }

        QDate start = QDate::fromString(fields.at(0), Qt::ISODate);
        QDate end = QDate::fromString(fields.at(1), Qt::ISODate);

        bool percentageOk = false;
        double percentage = fields.at(2).toDouble(&percentageOk);

        if (!start.isValid() || !end.isValid()
            || start > end
            || !percentageOk
            || !std::isfinite(percentage)
            || percentage <= 0
            || percentage > 100)
        {
            error = QStringLiteral("Invalid season information.");
            return false;
        }

        if (fields.at(3) != QStringLiteral("true")
            && fields.at(3) != QStringLiteral("false"))
        {
            error = QStringLiteral("Invalid season status.");
            return false;
        }

        Season season;

        season.startDate = start;
        season.endDate = end;
        season.percentage = percentage;
        season.isActive =
            fields.at(3) == QStringLiteral("true");

        loaded.append(season);
    }

    seasons = loaded;
    return true;
}

bool saveSeasons(QString filePath, const QVector<Season> &seasons,
    QString &error)
{
    error.clear();

    QFileInfo info(filePath);

    if (!QDir().mkpath(info.absolutePath()))
    {
        error = QStringLiteral("Cannot create Data folder.");
        return false;
    }

    QSaveFile file(filePath);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        error = QStringLiteral("Cannot write Seasons.txt.");
        return false;
    }

    QTextStream out(&file);

    for (const Season &season : seasons)
    {
        if (!season.startDate.isValid()
            || !season.endDate.isValid()
            || season.startDate > season.endDate
            || !std::isfinite(season.percentage)
            || season.percentage <= 0
            || season.percentage > 100)
        {
            error = QStringLiteral("Cannot save invalid season.");
            file.cancelWriting();
            return false;
        }

        QString status = season.isActive
            ? QStringLiteral("true")
            : QStringLiteral("false");

        QString line = makeRecord({
            season.startDate.toString(Qt::ISODate),
            season.endDate.toString(Qt::ISODate),
            QString::number(season.percentage, 'g', 15),
            status
        });

        out << line << '\n';
    }

    out.flush();

    if (out.status() != QTextStream::Ok || !file.commit())
    {
        error = QStringLiteral("Failed to save Seasons.txt.");
        return false;
    }

    return true;
}
}
