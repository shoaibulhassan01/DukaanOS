#include "bill_service.h"

namespace Pos
{
// TODO: Calculate and fill one product row for the bill.
bool calculateBillLine(QString billId, Order order, Product product,
    int quantity, Bill &bill, QString &error)
{
    error = QStringLiteral("TODO: Calculate the bill line.");
    return false;
}
}
