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

bool calculateBillLine(QString billId, Product product, int quantity,
    BillLine &line, QString &error)
{
    error.clear();
    line = BillLine{};

    if (quantity <= 0)
    {
        error = QStringLiteral("Quantity must be at least 1.");
        return false;
    }

    if (product.acronym.trimmed().isEmpty()
        || !std::isfinite(product.unitPrice)
        || !std::isfinite(product.tax)
        || !std::isfinite(product.discountPerUnit)
        || product.unitPrice < 0
        || product.tax < 0
        || product.discountPerUnit < 0
        || product.discountPerUnit > product.unitPrice)
    {
        error = QStringLiteral(
            "Product acronym, price, tax or discount is invalid.");
        return false;
    }

    BillLine calculated;
    calculated.detail.billId = billId;
    calculated.detail.productAcronym = product.acronym;
    calculated.detail.unitPrice = round2(product.unitPrice);
    calculated.detail.quantity = quantity;
    calculated.detail.totalDiscount =
        round2(round2(product.discountPerUnit) * quantity);
    calculated.totalWithoutTax =
        round2(calculated.detail.unitPrice * quantity);
    calculated.totalTax = round2(round2(product.tax) * quantity);
    calculated.totalPayable = round2(calculated.totalWithoutTax
        + calculated.totalTax - calculated.detail.totalDiscount);

    if (!std::isfinite(calculated.totalWithoutTax)
        || !std::isfinite(calculated.totalTax)
        || !std::isfinite(calculated.detail.totalDiscount)
        || !std::isfinite(calculated.totalPayable)
        || calculated.totalPayable < 0)
    {
        error = QStringLiteral("Bill totals are outside the supported range.");
        return false;
    }

    line = calculated;
    return true;
}

bool buildBill(QString billId, Order order, QVector<CartItem> items,
    Bill &bill, QString &error, double seasonalDiscountPercentage,
    double specialDiscountPercentage)
{
    error.clear();
    bill = Bill{};
    if (items.isEmpty())
    {
        error = QStringLiteral("Cannot build a bill from an empty cart.");
        return false;
    }

    Bill built;
    built.id = billId;
    built.customerName = order.customerName;
    built.orderId = order.id;
    built.paymentStatus = order.status;

    for (const CartItem &item : items)
    {
        if (item.product.id.trimmed().isEmpty()
            || item.product.barcode.trimmed().isEmpty())
        {
            error = QStringLiteral(
                "Cart item is missing its product ID or barcode.");
            return false;
        }

        BillLine line;
        if (!calculateBillLine(billId, item.product, item.quantity, line, error))
            return false;

        built.details.append(line.detail);
        built.totalWithoutTax += line.totalWithoutTax;
        built.totalTax += line.totalTax;
        built.totalDiscount += line.detail.totalDiscount;
    }

    built.totalWithoutTax = round2(built.totalWithoutTax);
    built.totalTax = round2(built.totalTax);
    built.totalDiscount = round2(built.totalDiscount);
    built.totalPayable = round2(
        built.totalWithoutTax + built.totalTax - built.totalDiscount);
    if (!std::isfinite(seasonalDiscountPercentage)
        || seasonalDiscountPercentage < 0
        || seasonalDiscountPercentage > 100)
    {
        error = QStringLiteral("Seasonal discount percentage is invalid.");
        return false;
    }
    if (seasonalDiscountPercentage > 0)
    {
        const double seasonalBase = round2(
            built.totalWithoutTax + built.totalTax - built.totalDiscount);
        built.seasonalDiscount = round2(
            seasonalBase * seasonalDiscountPercentage / 100.0);
        built.totalPayable = round2(
            seasonalBase - built.seasonalDiscount);
    }
    // Req 8: special discount "not more than 8 percent"; any amount from
    // 0.01 to 8.00 (2 decimals), 0 for none.
    if (!std::isfinite(specialDiscountPercentage)
        || specialDiscountPercentage < 0 || specialDiscountPercentage > 8
        || std::abs(specialDiscountPercentage * 100.0
               - std::round(specialDiscountPercentage * 100.0)) > 1e-6)
    {
        error = QStringLiteral(
            "Special discount must be from 0 to 8 percent, with at most 2 decimals.");
        return false;
    }
    if (specialDiscountPercentage > 0)
    {
        const double specialDiscountBase = built.totalPayable;
        built.specialDiscount = round2(
            specialDiscountBase * specialDiscountPercentage / 100.0);
        built.totalPayable = round2(
            specialDiscountBase - built.specialDiscount);
    }
    if (!std::isfinite(built.totalWithoutTax)
        || !std::isfinite(built.totalTax)
        || !std::isfinite(built.totalDiscount)
        || !std::isfinite(built.seasonalDiscount)
        || !std::isfinite(built.specialDiscount)
        || !std::isfinite(built.totalPayable))
    {
        error = QStringLiteral("Bill totals are outside the supported range.");
        return false;
    }

    bill = built;
    return true;
}
}
