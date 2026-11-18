#include "date_format.h"

namespace Pos
{
QString formatOrderDate(const QDateTime &when)
{
    return when.toString(QStringLiteral("yyyy-MM-dd"));
}

QString formatOrderTime(const QDateTime &when)
{
    return when.toString(QStringLiteral("HH:mm:ss"));
}
}
