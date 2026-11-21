#pragma once

#include <QDateTime>
#include <QString>

namespace Pos
{
// Order date as stored in Order.txt, e.g. 2026-11-19.
QString formatOrderDate(const QDateTime &when);

// Order time as stored in Order.txt, e.g. 14:05:09 (24 hour).
QString formatOrderTime(const QDateTime &when);
}
