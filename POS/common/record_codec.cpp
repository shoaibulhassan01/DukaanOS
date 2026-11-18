#include "record_codec.h"

namespace Pos
{
// Shared by Shoaib, Umar, Zain, Anas and Ahad - split a '#' terminated record into fields.
bool splitRecord(QString line, QStringList &fields)
{
    line = line.trimmed();
    if (!line.endsWith(QLatin1Char('#')))
        return false;

    line.chop(1);
    fields = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    return true;
}

// Shared by Shoaib, Umar, Zain, Anas and Ahad - join fields and append '#'.
QString makeRecord(QStringList fields)
{
    return fields.join(QLatin1Char(' ')) + QLatin1Char('#');
}
}
