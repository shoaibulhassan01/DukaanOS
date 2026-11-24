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

namespace Pos
{
namespace
{
// Every sold quantity must still be available in Product.txt (it may have
// changed since the item was added to the cart).
bool checkStockForSale(const QVector<CartItem> &items, QString &error)
{
    QVector<Product> products;
    if (!readProducts(DataPaths::products(), products, error))
        return false;
    for (const CartItem &item : items)
    {
        int needed = 0;
        for (const CartItem &other : items)
        {
            if (other.product.id == item.product.id)
                needed += other.quantity;
        }
        bool found = false;
        for (const Product &product : products)
        {
            if (product.id != item.product.id)
                continue;
            found = true;
            if (needed > product.quantity)
            {
                error = QStringLiteral("Only %1 of %2 left in stock.")
                    .arg(product.quantity).arg(product.id);
                return false;
            }
        }
        if (!found)
        {
            error = QStringLiteral("Product %1 is no longer in Product.txt.").arg(item.product.id);
            return false;
        }
    }
    return true;
}

void setErrorIfEmpty(QString &error, const QString &fallback)
{
    if (error.isEmpty())
        error = fallback;
}

// Totals of the whole cart, computed with the same bill-line rules.
bool computeTotals(const QVector<CartItem> &items, CartTotals &totals,
    QString &error, double seasonalDiscountPercentage = 0)
{
    Bill bill;
    if (!buildBill(QString(), Order(), items, bill, error,
            seasonalDiscountPercentage))
        return false;

    CartTotals sum;
    sum.withoutTax = bill.totalWithoutTax;
    sum.tax = bill.totalTax;
    sum.discount = bill.totalDiscount;
    sum.seasonalDiscount = bill.seasonalDiscount;
    sum.payable = bill.totalPayable;
    if (!std::isfinite(sum.withoutTax) || !std::isfinite(sum.tax)
        || !std::isfinite(sum.discount) || !std::isfinite(sum.payable)) {
        error = QStringLiteral("Cart totals are outside the supported range.");
        return false;
    }

    sum.withoutTax = std::round(sum.withoutTax * 100.0) / 100.0;
    sum.tax = std::round(sum.tax * 100.0) / 100.0;
    sum.discount = std::round(sum.discount * 100.0) / 100.0;
    sum.seasonalDiscount = std::round(sum.seasonalDiscount * 100.0) / 100.0;
    sum.payable = std::round(sum.payable * 100.0) / 100.0;
    if (!std::isfinite(sum.withoutTax) || !std::isfinite(sum.tax)
        || !std::isfinite(sum.discount) || !std::isfinite(sum.seasonalDiscount)
        || !std::isfinite(sum.payable)) {
        error = QStringLiteral("Cart totals are outside the supported range.");
        return false;
    }

    totals = sum;
    return true;
}
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
    double seasonalDiscountPercentage = 0;
    Season activeSeason;
    QString seasonError;
    if (findActiveSeason(now.date(), activeSeason, seasonError))
        seasonalDiscountPercentage = activeSeason.percentage;

    CartTotals totals;
    if (!computeTotals(request.items, totals, error,
            seasonalDiscountPercentage)) {
        setErrorIfEmpty(error, QStringLiteral("Could not calculate the cart totals."));
        return false;
    }

    std::unique_ptr<PaymentMethod> payment = createPaymentMethod(request.paymentMode);
    if (!payment) {
        error = QStringLiteral("Unknown payment mode: %1").arg(request.paymentMode);
        return false;
    }
    if (!payment->validate(request.amountPaid, totals.payable,
            request.merchant, error)) {
        setErrorIfEmpty(error, QStringLiteral("Payment validation failed."));
        return false;
    }

    // Reuse a held order when checking it out; otherwise create a new order.
    Order order;
    const bool resuming = !request.existingOrderId.trimmed().isEmpty();
    if (resuming)
    {
        QVector<Order> orders;
        if (!readOrders(DataPaths::orders(), orders, error))
            return false;
        bool found = false;
        for (const Order &existing : orders)
        {
            if (existing.id != request.existingOrderId)
                continue;
            if (existing.status != QStringLiteral("on-hold"))
            {
                error = QStringLiteral("Only an on-hold order can be resumed.");
                return false;
            }
            order = existing;
            found = true;
            break;
        }
        if (!found)
        {
            error = QStringLiteral("Held order %1 was not found.")
                .arg(request.existingOrderId);
            return false;
        }
    }
    else
    {
        if (!generateOrderId(DataPaths::orders(), now.date(), order.id, error))
        {
            setErrorIfEmpty(error, QStringLiteral("Could not generate an order ID."));
            return false;
        }
        order.date = formatOrderDate(now);
        order.time = formatOrderTime(now);
    }

    // Bill IDs are generated from the 9-digit IDs already present in Bill.txt.
    QString billId;
    if (!generateBillId(DataPaths::bills(), now.date(), billId, error)) {
        setErrorIfEmpty(error, QStringLiteral("Could not generate a bill ID."));
        return false;
    }

    // Cash has no merchant, so it is stored as an empty value in the model.
    if (!request.customerName.trimmed().isEmpty())
        order.customerName = request.customerName.trimmed();
    if (order.customerName.trimmed().isEmpty())
        order.customerName = QStringLiteral("Walk-in");
    if (!request.customerContact.trimmed().isEmpty())
        order.customerContact = request.customerContact.trimmed();
    order.status = QStringLiteral("paid");
    order.paymentAmount = totals.payable;
    order.paymentMode = payment->mode();
    order.merchant = payment->requiresMerchant()
        ? request.merchant.trimmed() : QString();

    QVector<OrderDetail> details;
    for (const CartItem &item : request.items) {
        OrderDetail detail;
        detail.orderId = order.id;
        detail.productId = item.product.id;
        detail.barcode = item.product.barcode;
        detail.quantity = item.quantity;
        details.append(detail);
    }

    // Step 3: build the one-line bill header and its product details.
    Bill bill;
    if (!buildBill(billId, order, request.items, bill, error,
            seasonalDiscountPercentage)) {
        setErrorIfEmpty(error, QStringLiteral("Could not build the bill."));
        return false;
    }
    bill.amountReceived = request.amountPaid;
    bill.balanceReturned = std::round(
        (request.amountPaid - totals.payable) * 100.0) / 100.0;
    bill.date = formatOrderDate(now);
    bill.time = formatOrderTime(now);

    // Stock is checked against Product.txt now, before any file is written,
    // so reduceStock cannot fail after the order and bill are already saved.
    if (!checkStockForSale(request.items, error))
        return false;

    // Steps 4-6: files are written from here on.
    if (resuming) {
        if (!updateOrder(DataPaths::orders(), DataPaths::orderDetail(),
                order, details, error)) {
            setErrorIfEmpty(error, QStringLiteral("Could not update the held order."));
            return false;
        }
    }
    else if (!saveOrder(DataPaths::orders(), DataPaths::orderDetail(),
                 order, details, error)) {
        setErrorIfEmpty(error, QStringLiteral("Could not save the order."));
        return false;
    }
    if (!saveBill(DataPaths::bills(), DataPaths::billDetails(), bill, error)) {
        setErrorIfEmpty(error, QStringLiteral("Could not save the bill."));
        return false;
    }
    if (!reduceStock(DataPaths::products(), request.items, error)) {
        setErrorIfEmpty(error, QStringLiteral("Could not reduce product stock."));
        return false;
    }

    result.order = order;
    result.details = details;
    result.bill = bill;
    result.totals = totals;
    return true;
}
}
