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
    bill = Bill{};

    if (quantity <= 0) {
        error = QStringLiteral("Quantity must be at least 1.");
        return false;
    }
    if (!std::isfinite(product.unitPrice) || !std::isfinite(product.tax)
        || !std::isfinite(product.discountPerUnit) || product.unitPrice < 0
        || product.tax < 0 || product.discountPerUnit < 0) {
        error = QStringLiteral("Product price, tax and discount must be finite and non-negative.");
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
    line.totalWithoutTax = round2(line.unitPrice * quantity);
    line.totalTax = round2(round2(product.tax) * quantity);
    line.totalDiscount = round2(line.discountPerUnit * quantity);
    line.totalPayable = round2(line.totalWithoutTax + line.totalTax
        - line.totalDiscount);
    if (!std::isfinite(line.totalWithoutTax) || !std::isfinite(line.totalTax)
        || !std::isfinite(line.totalDiscount) || !std::isfinite(line.totalPayable)
        || line.totalPayable < 0) {
        error = QStringLiteral("Bill totals are outside the supported range.");
        return false;
    }
    line.paymentStatus = order.status;

    bill = line;
    return true;
}

bool buildBill(QString billId, Order order, QVector<CartItem> items,
    QVector<Bill> &rows, QString &error)
{
    error.clear();
    rows.clear();
    if (items.isEmpty()) {
        error = QStringLiteral("Cannot build a bill from an empty cart.");
        return false;
    }

    QVector<Bill> built;
    for (const CartItem &item : items) {
        if (item.product.id.trimmed().isEmpty()
            || item.product.barcode.trimmed().isEmpty()
            || item.product.acronym.trimmed().isEmpty()) {
            error = QStringLiteral("Cart item is missing its product ID, barcode or acronym.");
            return false;
        }

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
