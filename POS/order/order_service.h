#pragma once

#include "../checkout/checkout_service.h"

namespace Pos
{
// Owner: Abdul Ahad - check stock and add a scanned item to the cart.
bool addProductToOrder(QString barcode, int quantity, QString &error);

// Owner: Abdul Ahad - remove an item from the cart.
bool removeProductFromOrder(QString productId, QString &error);

// Owner: Abdul Ahad - save the current order with status "on-hold".
bool holdOrder(QString &error);

// Owner: Abdul Ahad - load a held order back into the cart.
bool resumeOrder(QString orderId, QString &error);

bool completePayment(const CheckoutRequest &request,
    CheckoutResult &result, QString &error);
}
