#pragma once

#include <QString>

namespace Pos
{
// Owner: Zain Ul Abideen - make a unique 8-digit order ID.
bool generateOrderId(QString &id);

// Owner: Zain Ul Abideen - make a unique 9-digit bill ID.
bool generateBillId(QString &id);
}
