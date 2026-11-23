#include "date_format.h"

namespace Pos
{
QString formatOrderDate(const QDateTime &dateTime)
{
    return dateTime.date().toString(Qt::ISODate);
}

QString formatOrderTime(const QDateTime &dateTime)
{
    return dateTime.time().toString(QStringLiteral("HH:mm:ss"));
}
}
