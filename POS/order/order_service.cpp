#include "order_service.h"

namespace Pos
{
// TODO: Check stock and add a scanned item to the cart.
bool addProductToOrder(QString barcode, int quantity, QString &error)
{
    error = QStringLiteral("TODO: Add product to order.");
    return false;
}

// TODO: Remove an item from the cart.
bool removeProductFromOrder(QString productId, QString &error)
{
    error = QStringLiteral("TODO: Remove product from order.");
    return false;
}

// TODO: Save the current order with status "on-hold".
bool holdOrder(QString &error)
{
    error = QStringLiteral("TODO: Hold the current order.");
    return false;
}

// TODO: Load a held order back into the cart.
bool resumeOrder(QString orderId, QString &error)
{
    error = QStringLiteral("TODO: Resume the held order.");
    return false;
}

// TODO: Coordinate checkout with payment processing and create bill rows.
bool completePayment(QString paymentMode, QString merchant,
    double amountPaid, QVector<Bill> &billRows, QString &error)
{
    error = QStringLiteral("TODO: Complete payment and generate bill.");
    return false;
}
}
