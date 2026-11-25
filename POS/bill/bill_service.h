#pragma once

#include "bill.h"

#include "../order/cart_item.h"
#include "../order/order.h"
#include "../product/product.h"

#include <QString>
#include <QVector>

namespace Pos
{
// Calculates a product's contribution to the bill and its BillDetail record.
bool calculateBillLine(QString billId, Product product, int quantity,
    BillLine &line, QString &error);

// Builds a single Bill header and one detail record per cart item.
bool buildBill(QString billId, Order order, QVector<CartItem> items,
    Bill &bill, QString &error, double seasonalDiscountPercentage = 0,
    double specialDiscountPercentage = 0);
}
