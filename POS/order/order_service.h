#pragma once

#include "../bill/bill.h"

namespace Pos
{
// Owner: Ahad. TODO: Check stock and add a scanned item to the cart.
bool addProductToOrder(QString barcode, int quantity, QString &error);

// Owner: Ahad. TODO: Remove an item from the cart.
bool removeProductFromOrder(QString productId, QString &error);

// Owner: Ahad. TODO: Save the current order with status "on-hold".
bool holdOrder(QString &error);

// Owner: Ahad. TODO: Load a held order back into the cart.
bool resumeOrder(QString orderId, QString &error);

// Owner: Ahad. TODO: Coordinate checkout with Anas's payment processing and create bill rows.
bool completePayment(QString paymentMode, QString merchant,
    double amountPaid, QVector<Bill> &billRows, QString &error);
}
