#pragma once

#include <QDate>
#include <QString>

namespace Pos
{
// Owner: Zain Ul Abideen - make a new 8-digit order ID that is not in
// orderFile. Requirement 8: the LAST TWO digits are the day of the month the
// order was made. Format: 6-digit running number + 2-digit day, e.g.
// 00004219 = the 42nd order, made on the 19th. A missing file means the
// first order.
bool generateOrderId(const QString &orderFile, const QDate &date, QString &id,
    QString &error);

// Owner: Zain Ul Abideen - make a new 9-digit bill ID that is not in
// billFile. Format: yyMMdd + 3-digit running number for that day, e.g.
// 261119004.
bool generateBillId(const QString &billFile, const QDate &date, QString &id,
    QString &error);
}
