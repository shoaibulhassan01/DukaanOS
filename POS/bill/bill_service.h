#pragma once

#include "bill.h"

#include "../order/cart_item.h"
#include "../order/order.h"
#include "../product/product.h"

#include <QString>
#include <QVector>

namespace Pos
{
// Owner: Ahad. Calculates and fills one product row for the bill.
bool calculateBillLine(QString billId, Order order, Product product,
    int quantity, Bill &bill, QString &error);

// Owner: Ahad. NEW. One bill row per cart item (uses calculateBillLine).
bool buildBill(QString billId, Order order, QVector<CartItem> items,
    QVector<Bill> &rows, QString &error);
}
