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
    QString &error)
{
    QVector<Bill> rows;
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

    sum.withoutTax = std::round(sum.withoutTax * 100.0) / 100.0;
    sum.tax = std::round(sum.tax * 100.0) / 100.0;
    sum.discount = std::round(sum.discount * 100.0) / 100.0;
    sum.payable = std::round(sum.payable * 100.0) / 100.0;
    if (!std::isfinite(sum.withoutTax) || !std::isfinite(sum.tax)
        || !std::isfinite(sum.discount) || !std::isfinite(sum.payable)) {
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

    CartTotals totals;
    if (!computeTotals(request.items, totals, error)) {
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

    // Step 1: order ID.
    const QDateTime now = QDateTime::currentDateTime();
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

    // Step 3: build the bill rows.
    QVector<Bill> billRows;
    if (!buildBill(billId, order, request.items, billRows, error)) {
        setErrorIfEmpty(error, QStringLiteral("Could not build the bill."));
        return false;
    }

    // Stock is checked against Product.txt now, before any file is written,
    // so reduceStock cannot fail after the order and bill are already saved.
    if (!checkStockForSale(request.items, error))
        return false;

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
