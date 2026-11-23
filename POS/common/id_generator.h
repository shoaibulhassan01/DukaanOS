#pragma once

#include <QString>
#include <QDate>

namespace Pos
{
// Owner: Zain Ul Abideen - make a unique 8-digit order ID.
bool generateOrderId(QString &id);

// Owner: Zain Ul Abideen - make a unique 9-digit bill ID.
bool generateBillId(QString &id);

bool generateOrderId(QString filePath, QDate date, QString &id, QString &error);
bool generateBillId(QString filePath, QDate date, QString &id, QString &error);
}
