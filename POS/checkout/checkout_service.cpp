#include "checkout_service.h"

#include "../bill/bill_repository.h"
#include "../bill/bill_service.h"
#include "../common/data_paths.h"
#include "../common/date_format.h"
#include "../common/id_generator.h"
#include "../order/order_repository.h"
#include "../payment/payment_method.h"
#include "../product/product_repository.h"
#include "../product/product_service.h"
#include "../season/season_service.h"

#include <cmath>
#include <memory>
#include <QDateTime>
#include <QVector>

namespace Pos
{
namespace
{
void setErrorIfEmpty(QString &error, const QString &fallback)
{
    if (error.isEmpty())
        error = fallback;
}

bool checkStock(const QVector<CartItem> &items, QString &error)
{
    QVector<Product> products;
    if (!readProducts(DataPaths::products(), products, error))
        return false;
    for (int i = 0; i < items.size(); ++i)
    {
        int needed = 0;
        for (const CartItem &item : items)
        {
            if (item.product.id == items.at(i).product.id)
                needed += item.quantity;
        }
        bool found = false;
        for (const Product &product : products)
        {
            if (product.id != items.at(i).product.id)
                continue;
            found = true;
            if (needed <= 0 || needed > product.quantity)
            {
                error = QStringLiteral("Only %1 of %2 left in stock.")
                    .arg(product.quantity).arg(product.id);
                return false;
            }
            break;
        }
        if (!found)
        {
            error = QStringLiteral("Product %1 is no longer in Product.txt.")
                .arg(items.at(i).product.id);
            return false;
        }
    }
    return true;
}

// Totals of the whole cart, computed with the same bill-line rules.
double round2(double value)
{
    return std::round(value * 100.0) / 100.0;
}

bool allocateDiscount(double amount, QVector<Bill> &rows, bool seasonal,
    QString &error)
{
    double remaining = round2(amount);
    double basis = 0;
    for (const Bill &row : rows)
        basis += row.totalPayable;
    if (remaining < 0 || remaining > round2(basis))
    {
        error = QStringLiteral("Discount exceeds the remaining bill amount.");
        return false;
    }

    for (int i = 0; i < rows.size(); ++i)
    {
        Bill &row = rows[i];
        const double allocation = i == rows.size() - 1
            ? remaining
            : round2(amount * row.totalPayable / basis);
        const double bounded = qMin(remaining, qMin(row.totalPayable, allocation));
        if (seasonal)
            row.seasonalDiscount = bounded;
        else
            row.specialDiscount = bounded;
        row.totalPayable = round2(row.totalPayable - bounded);
        remaining = round2(remaining - bounded);
    }
    if (remaining != 0)
    {
        error = QStringLiteral("Could not allocate the bill discount exactly.");
        return false;
    }
    return true;
}

bool computeTotalsForDate(const QVector<CartItem> &items, double specialPercent,
    QDate date, QVector<Bill> &rows, CartTotals &totals, QString &error)
{
    rows.clear();
    totals = CartTotals{};
    if (!std::isfinite(specialPercent) || specialPercent < 0 || specialPercent > 8)
    {
        error = QStringLiteral("Special discount must be between 0 and 8 percent.");
        return false;
    }
    if (!buildBill(QString(), Order(), items, rows, error))
        return false;

    CartTotals sum;
    for (const Bill &row : rows) {
        sum.withoutTax += row.totalWithoutTax;
        sum.tax += row.totalTax;
        sum.discount += row.totalDiscount;
        sum.payable += row.totalPayable;
    }
    if (!std::isfinite(sum.withoutTax) || !std::isfinite(sum.tax)
        || !std::isfinite(sum.discount) || !std::isfinite(sum.payable)) {
        error = QStringLiteral("Cart totals are outside the supported range.");
        return false;
    }

    sum.withoutTax = round2(sum.withoutTax);
    sum.tax = round2(sum.tax);
    sum.discount = round2(sum.discount);
    sum.payable = round2(sum.payable);
    if (!std::isfinite(sum.withoutTax) || !std::isfinite(sum.tax)
        || !std::isfinite(sum.discount) || !std::isfinite(sum.payable)) {
        error = QStringLiteral("Cart totals are outside the supported range.");
        return false;
    }

    Season activeSeason;
    QString seasonError;
    const bool hasSeason = findActiveSeason(date, activeSeason, seasonError);
    if (!seasonError.isEmpty())
    {
        error = seasonError;
        return false;
    }
    const double discountableBase = round2(sum.withoutTax - sum.discount);
    const double seasonalAmount = hasSeason
        ? round2(discountableBase * activeSeason.percentage / 100.0) : 0;
    if (seasonalAmount > 0
        && !allocateDiscount(seasonalAmount, rows, true, error))
        return false;

    const double remainingAfterSeason = round2(discountableBase - seasonalAmount);
    const double specialAmount = round2(remainingAfterSeason * specialPercent / 100.0);
    if (specialAmount > 0
        && !allocateDiscount(specialAmount, rows, false, error))
        return false;

    sum.seasonalDiscount = seasonalAmount;
    sum.specialDiscount = specialAmount;
    sum.discount = round2(sum.discount + seasonalAmount + specialAmount);
    sum.payable = round2(sum.payable - seasonalAmount - specialAmount);
    if (!std::isfinite(sum.discount) || !std::isfinite(sum.payable)
        || sum.payable < 0)
    {
        error = QStringLiteral("Cart totals are outside the supported range.");
        return false;
    }
    totals = sum;
    error.clear();
    return true;
}
}

bool calculateCheckoutTotals(const QVector<CartItem> &items,
    double specialDiscountPercent, CartTotals &totals, QString &error)
{
    QVector<Bill> rows;
    return computeTotalsForDate(items, specialDiscountPercent,
        QDate::currentDate(), rows, totals, error);
}

bool generateBill(const CheckoutRequest &request, CheckoutResult &result,
    QString &error)
{
    error.clear();
    result = CheckoutResult{};

    // Validate the request and payment (no files touched).
    if (request.items.isEmpty()) {
        error = QStringLiteral("The cart is empty.");
        return false;
    }

    const QDateTime now = QDateTime::currentDateTime();
    CartTotals totals;
    QVector<Bill> billRows;
    if (!computeTotalsForDate(request.items, request.specialDiscountPercent,
            now.date(), billRows, totals, error)) {
        setErrorIfEmpty(error, QStringLiteral("Could not calculate the cart totals."));
        return false;
    }

    std::unique_ptr<PaymentMethod> payment = createPaymentMethod(request.paymentMode);
    if (!payment) {
        error = QStringLiteral("Unknown payment mode: %1").arg(request.paymentMode);
        return false;
    }
    const double amountReceived = round2(request.amountPaid);
    if (!payment->validate(amountReceived, totals.payable,
            request.merchant, error)) {
        setErrorIfEmpty(error, QStringLiteral("Payment validation failed."));
        return false;
    }
    if (!checkStock(request.items, error))
        return false;

    // Step 1: order ID.
    QString orderId;
    if (!generateOrderId(DataPaths::orders(), now.date(), orderId, error)) {
        setErrorIfEmpty(error, QStringLiteral("Could not generate an order ID."));
        return false;
    }

    // Step 2: bill ID.
    QString billId;
    if (!generateBillId(DataPaths::bills(), now.date(), billId, error)) {
        setErrorIfEmpty(error, QStringLiteral("Could not generate a bill ID."));
        return false;
    }

    // Order record. Cash has no merchant, so it is stored as "-".
    Order order;
    order.id = orderId;
    order.date = formatOrderDate(now);
    order.time = formatOrderTime(now);
    order.customerName = request.customerName.trimmed().isEmpty()
        ? QStringLiteral("Walk-in") : request.customerName.trimmed();
    order.customerContact = request.customerContact.trimmed();
    order.status = QStringLiteral("paid");
    order.paymentAmount = totals.payable;
    order.paymentMode = payment->mode();
    order.merchant = payment->requiresMerchant()
        ? request.merchant.trimmed() : QString();

    QVector<OrderDetail> details;
    for (const CartItem &item : request.items) {
        OrderDetail detail;
        detail.orderId = orderId;
        detail.productId = item.product.id;
        detail.barcode = item.product.barcode;
        detail.quantity = item.quantity;
        details.append(detail);
    }

    // Step 3: add the generated bill ID and settlement to each item row.
    const double balance = round2(amountReceived - totals.payable);
    for (Bill &row : billRows) {
        row.id = billId;
        row.customerName = order.customerName;
        row.orderId = order.id;
        row.paymentStatus = order.status;
        row.amountReceived = amountReceived;
        row.balanceReturned = balance;
    }

    // Steps 4-6: files are written from here on.
    if (!saveOrder(DataPaths::orders(), DataPaths::orderDetail(), order, details, error)) {
        setErrorIfEmpty(error, QStringLiteral("Could not save the order."));
        return false;
    }
    if (!saveBill(DataPaths::bills(), billRows, error)) {
        setErrorIfEmpty(error, QStringLiteral("Could not save the bill."));
        return false;
    }
    if (!reduceStock(DataPaths::products(), request.items, error)) {
        setErrorIfEmpty(error, QStringLiteral("Could not reduce product stock."));
        return false;
    }

    result.order = order;
    result.details = details;
    result.billRows = billRows;
    result.totals = totals;
    return true;
}
}
