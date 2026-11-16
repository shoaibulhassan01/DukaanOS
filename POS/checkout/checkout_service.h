#pragma once

#include "../bill/bill.h"
#include "../order/cart_item.h"
#include "../order/order.h"
#include "../order/order_detail.h"

#include <QString>
#include <QVector>

namespace Pos
{
// Owner: Ahad. NEW.
struct CheckoutRequest
{
    QString customerName, customerContact, paymentMode, merchant;
    double amountPaid = 0;
    QVector<CartItem> items;
};

struct CheckoutResult
{
    Order order;
    QVector<OrderDetail> details;
    QVector<Bill> billRows;
    CartTotals totals;
};

// Runs the whole checkout: validate payment, order ID, bill ID, build bill,
// save order, save bill, reduce stock. If a step fails the error is returned
// and nothing after it runs. Steps 1-3 only compute; files are written from
// the save step on.
bool generateBill(const CheckoutRequest &request, CheckoutResult &result,
    QString &error);
}
