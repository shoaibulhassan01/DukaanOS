#include "checkout_service.h"

#include "../bill/bill_repository.h"
#include "../bill/bill_service.h"
#include "../common/data_paths.h"
#include "../common/date_format.h"
#include "../common/id_generator.h"
#include "../order/order_repository.h"
#include "../payment/payment_method.h"
#include "../product/product_service.h"

#include <QDate>
#include <memory>
#include <QDateTime>

namespace Pos
{
namespace
{
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
    totals = sum;
    return true;
}
}

bool generateBill(const CheckoutRequest &request, CheckoutResult &result,
    QString &error)
{
    error.clear();

    // Validate the request and payment (no files touched).
    if (request.items.isEmpty()) {
        error = QStringLiteral("The cart is empty.");
        return false;
    }

    CartTotals totals;
    if (!computeTotals(request.items, totals, error))
        return false;

    std::unique_ptr<PaymentMethod> payment = createPaymentMethod(request.paymentMode);
    if (!payment) {
        error = QStringLiteral("Unknown payment mode: %1").arg(request.paymentMode);
        return false;
    }
    if (!payment->validate(request.amountPaid, totals.payable,
            request.merchant, error))
        return false;

    // Step 1: order ID.
    const QDateTime now = QDateTime::currentDateTime();
    QString orderId;
    if (!generateOrderId(DataPaths::orders, now.date(), orderId, error))
        return false;

    // Step 2: bill ID.
    QString billId;
    if (!generateBillId(DataPaths::bills, billId, error))
        return false;

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
    if (!buildBill(billId, order, request.items, billRows, error))
        return false;

    // Steps 4-6: files are written from here on.
    if (!saveOrder(DataPaths::orders, DataPaths::orderDetail, order, details, error))
        return false;
    if (!saveBill(DataPaths::bills, billRows, error))
        return false;
    if (!reduceStock(DataPaths::products, request.items, error))
        return false;

    result.order = order;
    result.details = details;
    result.billRows = billRows;
    result.totals = totals;
    return true;
}
}
