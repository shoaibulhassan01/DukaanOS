#pragma once

#include <QDateTime>
#include <QString>

namespace Pos
{
QString formatOrderDate(const QDateTime &dateTime);
QString formatOrderTime(const QDateTime &dateTime);
}
