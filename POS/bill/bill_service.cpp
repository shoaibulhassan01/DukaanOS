#include "bill_service.h"

#include <cmath>

namespace Pos
{
namespace
{
double round2(double value)
{
    return std::round(value * 100.0) / 100.0;
}
}

// totalWithoutTax = unitPrice x qty, totalTax = tax x qty,
// totalDiscount = discount x qty, totalPayable = without tax + tax - discount.
bool calculateBillLine(QString billId, Order order, Product product,
    int quantity, Bill &bill, QString &error)
{
    error.clear();

    if (quantity <= 0) {
        error = QStringLiteral("Quantity must be at least 1.");
        return false;
    }
    if (product.unitPrice < 0 || product.tax < 0 || product.discountPerUnit < 0) {
        error = QStringLiteral("Product price, tax and discount cannot be negative.");
        return false;
    }

    Bill line;
    line.id = billId;
    line.customerName = order.customerName;
    line.orderId = order.id;
    line.productAcronym = product.acronym;
    line.unitPrice = round2(product.unitPrice);
    line.discountPerUnit = round2(product.discountPerUnit);
    line.quantity = quantity;
    line.totalWithoutTax = round2(product.unitPrice * quantity);
    line.totalTax = round2(product.tax * quantity);
    line.totalDiscount = round2(product.discountPerUnit * quantity);
    line.totalPayable = round2(line.totalWithoutTax + line.totalTax
        - line.totalDiscount);
    line.paymentStatus = order.status;

    bill = line;
    return true;
}

bool buildBill(QString billId, Order order, QVector<CartItem> items,
    QVector<Bill> &rows, QString &error)
{
    error.clear();
    if (items.isEmpty()) {
        error = QStringLiteral("Cannot build a bill from an empty cart.");
        return false;
    }

    QVector<Bill> built;
    for (const CartItem &item : items) {
        Bill line;
        if (!calculateBillLine(billId, order, item.product, item.quantity,
                line, error))
            return false;
        built.append(line);
    }

    rows = built;
    return true;
}
}
