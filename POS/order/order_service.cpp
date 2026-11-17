#include "order_service.h"

namespace Pos
{
// Owner: Abdul Ahad - check stock and add a scanned item to the cart.
bool addProductToOrder(QString barcode, int quantity, QString &error)
{
    error = QStringLiteral("TODO: Add product to order.");
    return false;
}

// Owner: Abdul Ahad - remove an item from the cart.
bool removeProductFromOrder(QString productId, QString &error)
{
    error = QStringLiteral("TODO: Remove product from order.");
    return false;
}

// Owner: Abdul Ahad - save the current order with status "on-hold".
bool holdOrder(QString &error)
{
    error = QStringLiteral("TODO: Hold the current order.");
    return false;
}

// Owner: Abdul Ahad - load a held order back into the cart.
bool resumeOrder(QString orderId, QString &error)
{
    error = QStringLiteral("TODO: Resume the held order.");
    return false;
}

// Owner: Abdul Ahad, with Anas for payment - complete checkout and create bill rows.
bool completePayment(QString paymentMode, QString merchant,
    double amountPaid, QVector<Bill> &billRows, QString &error)
{
    error = QStringLiteral("TODO: Complete payment and generate bill.");
    return false;
}
}
