#pragma once

#include "order_preview.h"
#include "../../POS/order/order.h"

#include <QString>

namespace UiPages
{
// Rebuilds the bill of a saved order from Bill.txt (prices and totals as they
// were at the time of sale), OrderDetail.txt and Product.txt (product names),
// so Order History can show it again.
bool loadSavedBill(const Pos::Order &order, OrderPreview &bill, QString &error);
}
