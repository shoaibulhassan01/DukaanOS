#include "order_service.h"

#include "../checkout/checkout_service.h"
#include "../bill/bill_repository.h"
#include "../common/data_paths.h"
#include "../common/date_format.h"
#include "../common/id_generator.h"
#include "../order/order_repository.h"
#include "../product/product_repository.h"
#include "../product/product_service.h"

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <cmath>

namespace Pos
{
namespace
{
bool findOrder(const QString &orderId, Order &order, QString &error)
{
    QVector<Order> orders;
    if (!readOrders(DataPaths::orders(), orders, error))
        return false;
    for (const Order &candidate : orders)
    {
        if (candidate.id == orderId)
        {
            order = candidate;
            error.clear();
            return true;
        }
    }
    error = QStringLiteral("Order %1 was not found.").arg(orderId);
    return false;
}

bool checkEditPermission(const Order &order, bool managerApproved,
    QString &error)
{
    if (order.status == QStringLiteral("on-hold"))
    {
        error.clear();
        return true;
    }
    if (order.status != QStringLiteral("paid"))
    {
        error = QStringLiteral("Only paid or on-hold orders can be edited.");
        return false;
    }

    Bill bill;
    if (!readBillForOrder(DataPaths::bills(), DataPaths::billDetails(),
            order.id, bill, error))
        return false;
    if (bill.paymentStatus != QStringLiteral("paid"))
    {
        error = QStringLiteral("The order does not have a paid bill.");
        return false;
    }

    const QDate paidDate = QDate::fromString(bill.date, Qt::ISODate);
    const qint64 daysSincePayment = paidDate.daysTo(QDate::currentDate());
    if (!paidDate.isValid() || daysSincePayment < 0 || daysSincePayment > 7)
    {
        error = QStringLiteral("Paid orders can only be edited within 7 days of payment.");
        return false;
    }
    if (!managerApproved)
    {
        error = QStringLiteral("Manager approval is required to edit a paid order.");
        return false;
    }
    error.clear();
    return true;
}

QVector<OrderDetail> orderDetails(const QString &orderId,
    const QVector<CartItem> &items)
{
    QVector<OrderDetail> details;
    details.reserve(items.size());
    for (const CartItem &item : items)
    {
        OrderDetail detail;
        detail.orderId = orderId;
        detail.productId = item.product.id;
        detail.barcode = item.product.barcode;
        detail.quantity = item.quantity;
        details.append(detail);
    }
    return details;
}

bool readOrderReport(bool completed, OrderReport &report, QString &error)
{
    QVector<Order> allOrders;
    if (!readOrders(DataPaths::orders(), allOrders, error))
        return false;

    OrderReport result;
    for (const Order &order : allOrders)
    {
        const bool isCompleted = order.status == QStringLiteral("paid")
            || order.status == QStringLiteral("archived");
        if (isCompleted != completed)
            continue;
        result.orders.append(order);
        result.totalAmount += order.paymentAmount;
        if (!std::isfinite(result.totalAmount))
        {
            error = QStringLiteral("Order report total is outside the supported range.");
            return false;
        }
    }
    report = result;
    error.clear();
    return true;
}
}

bool readIncompleteOrdersReport(OrderReport &report, QString &error)
{
    return readOrderReport(false, report, error);
}

bool readCompletedOrdersReport(OrderReport &report, QString &error)
{
    return readOrderReport(true, report, error);
}

bool readHeldOrders(QVector<Order> &orders, QString &error)
{
    QVector<Order> allOrders;
    if (!readOrders(DataPaths::orders(), allOrders, error))
        return false;
    QVector<Order> held;
    for (const Order &order : allOrders)
    {
        if (order.status == QStringLiteral("on-hold"))
            held.append(order);
    }
    orders = held;
    error.clear();
    return true;
}

bool addProductToOrder(OrderSession &session, QString barcode, int quantity,
    QString &error)
{
    error.clear();
    if (quantity <= 0)
    {
        error = QStringLiteral("Quantity must be at least 1.");
        return false;
    }

    QVector<Product> products;
    if (!readProducts(DataPaths::products(), products, error))
        return false;
    const Product *matched = nullptr;
    for (const Product &product : products)
    {
        if (product.barcode == barcode)
        {
            matched = &product;
            break;
        }
    }
    if (!matched)
    {
        error = QStringLiteral("Barcode %1 was not found.").arg(barcode);
        return false;
    }

    int currentQuantity = 0;
    for (const CartItem &item : session.items)
    {
        if (item.product.id == matched->id)
            currentQuantity += item.quantity;
    }
    if (currentQuantity + quantity > matched->quantity)
    {
        error = QStringLiteral("Only %1 of %2 left in stock.")
            .arg(matched->quantity).arg(matched->id);
        return false;
    }

    for (CartItem &item : session.items)
    {
        if (item.product.id == matched->id)
        {
            item.product = *matched;
            item.quantity += quantity;
            return true;
        }
    }
    session.items.append(CartItem{*matched, quantity});
    return true;
}

bool removeProductFromOrder(OrderSession &session, QString productId,
    QString &error)
{
    for (int i = 0; i < session.items.size(); ++i)
    {
        if (session.items.at(i).product.id == productId)
        {
            session.items.removeAt(i);
            error.clear();
            return true;
        }
    }
    error = QStringLiteral("Product %1 is not in the order.").arg(productId);
    return false;
}

bool holdOrder(OrderSession &session, QString &error)
{
    error.clear();
    if (session.items.isEmpty())
    {
        error = QStringLiteral("Cannot hold an empty order.");
        return false;
    }

    const QDateTime now = QDateTime::currentDateTime();
    Order order;
    if (session.orderId.isEmpty())
    {
        if (!generateOrderId(DataPaths::orders(), now.date(), order.id, error))
            return false;
        order.date = formatOrderDate(now);
        order.time = formatOrderTime(now);
        order.paymentMode = QStringLiteral("cash");
    }
    else
    {
        if (!findOrder(session.orderId, order, error))
            return false;
        if (order.status != QStringLiteral("on-hold"))
        {
            error = QStringLiteral("Only an on-hold order can be saved again.");
            return false;
        }
    }

    if (!session.customerName.trimmed().isEmpty())
        order.customerName = session.customerName.trimmed();
    if (order.customerName.trimmed().isEmpty())
        order.customerName = QStringLiteral("Walk-in");
    order.customerContact = session.customerContact.trimmed();
    order.status = QStringLiteral("on-hold");
    order.paymentAmount = 0;
    order.merchant.clear();
    const QVector<OrderDetail> details = orderDetails(order.id, session.items);

    const bool saved = session.orderId.isEmpty()
        ? saveOrder(DataPaths::orders(), DataPaths::orderDetail(), order,
            details, error)
        : updateOrder(DataPaths::orders(), DataPaths::orderDetail(), order,
            details, error);
    if (!saved)
        return false;

    session.orderId = order.id;
    return true;
}

bool openOrderForEditing(QString orderId, bool managerApproved,
    OrderSession &session, QString &error)
{
    session = OrderSession{};
    error.clear();
    Order order;
    if (!findOrder(orderId, order, error)
        || !checkEditPermission(order, managerApproved, error))
        return false;

    QVector<OrderDetail> details;
    if (!readOrderDetails(DataPaths::orderDetail(), orderId, details, error))
        return false;
    if (details.isEmpty())
    {
        error = QStringLiteral("Order %1 has no saved products.").arg(orderId);
        return false;
    }

    QVector<Product> products;
    if (!readProducts(DataPaths::products(), products, error))
        return false;

    OrderSession restored;
    restored.orderId = order.id;
    restored.customerName = order.customerName;
    restored.customerContact = order.customerContact;
    for (const OrderDetail &detail : details)
    {
        int found = -1;
        for (int i = 0; i < products.size(); ++i)
        {
            if (products.at(i).id == detail.productId
                && products.at(i).barcode == detail.barcode)
            {
                found = i;
                break;
            }
        }
        if (found < 0)
        {
            error = QStringLiteral("Product %1 from order %2 is no longer available.")
                .arg(detail.productId, orderId);
            return false;
        }

        Product product = products.at(found);
        bool merged = false;
        for (CartItem &item : restored.items)
        {
            if (item.product.id == product.id)
            {
                item.quantity += detail.quantity;
                merged = true;
                break;
            }
        }
        if (!merged)
            restored.items.append(CartItem{product, detail.quantity});
    }

    session = restored;
    error.clear();
    return true;
}

bool resumeOrder(QString orderId, OrderSession &session, QString &error)
{
    Order order;
    if (!findOrder(orderId, order, error))
        return false;
    if (order.status != QStringLiteral("on-hold"))
    {
        error = QStringLiteral("Order %1 is not on hold.").arg(orderId);
        return false;
    }
    return openOrderForEditing(orderId, false, session, error);
}

bool saveOrderEdits(OrderSession &session, bool managerApproved,
    QString &error)
{
    if (session.orderId.trimmed().isEmpty() || session.items.isEmpty())
    {
        error = QStringLiteral("Choose an existing order with at least one product.");
        return false;
    }

    Order order;
    if (!findOrder(session.orderId, order, error)
        || !checkEditPermission(order, managerApproved, error))
        return false;

    QVector<OrderDetail> previousDetails;
    if (!readOrderDetails(DataPaths::orderDetail(), order.id, previousDetails, error))
        return false;
    if (previousDetails.isEmpty())
    {
        error = QStringLiteral("Order %1 has no saved products to replace.").arg(order.id);
        return false;
    }

    QVector<Product> products;
    if (!readProducts(DataPaths::products(), products, error))
        return false;

    QHash<QString, int> productIndex;
    for (int i = 0; i < products.size(); ++i)
    {
        if (productIndex.contains(products.at(i).id))
        {
            error = QStringLiteral("Product ID %1 is duplicated in Product.txt.")
                .arg(products.at(i).id);
            return false;
        }
        productIndex.insert(products.at(i).id, i);
    }

    QHash<QString, int> previousQuantity;
    for (const OrderDetail &detail : previousDetails)
    {
        const int index = productIndex.value(detail.productId, -1);
        if (index < 0 || products.at(index).barcode != detail.barcode)
        {
            error = QStringLiteral("Product %1 from order %2 is no longer available.")
                .arg(detail.productId, order.id);
            return false;
        }
        previousQuantity[detail.productId] += detail.quantity;
    }

    QVector<CartItem> updatedItems;
    QHash<QString, int> updatedQuantity;
    for (const CartItem &item : session.items)
    {
        if (item.quantity <= 0)
        {
            error = QStringLiteral("Every product quantity must be greater than zero.");
            return false;
        }
        const int index = productIndex.value(item.product.id, -1);
        if (index < 0 || products.at(index).barcode != item.product.barcode)
        {
            error = QStringLiteral("Product %1 is no longer available.")
                .arg(item.product.id);
            return false;
        }
        updatedQuantity[item.product.id] += item.quantity;
    }

    for (auto it = updatedQuantity.cbegin(); it != updatedQuantity.cend(); ++it)
    {
        const int index = productIndex.value(it.key());
        const int oldQuantity = previousQuantity.value(it.key());
        const int currentStock = products.at(index).quantity;
        const int available = order.status == QStringLiteral("paid")
            ? currentStock + oldQuantity : currentStock;
        if (it.value() > available)
        {
            error = QStringLiteral("Only %1 of %2 are available for this edit.")
                .arg(available).arg(it.key());
            return false;
        }
    }

    for (int i = 0; i < session.items.size(); ++i)
    {
        const CartItem &item = session.items.at(i);
        const int index = productIndex.value(item.product.id);
        bool merged = false;
        for (CartItem &updated : updatedItems)
        {
            if (updated.product.id == item.product.id)
            {
                updated.quantity += item.quantity;
                merged = true;
                break;
            }
        }
        if (!merged)
            updatedItems.append(CartItem{products.at(index), item.quantity});
    }

    QVector<Product> updatedProducts = products;
    if (order.status == QStringLiteral("paid"))
    {
        for (int i = 0; i < updatedProducts.size(); ++i)
        {
            const QString &productId = updatedProducts.at(i).id;
            updatedProducts[i].quantity += previousQuantity.value(productId)
                - updatedQuantity.value(productId);
        }
        if (!saveProducts(DataPaths::products(), updatedProducts, error))
            return false;
    }

    order.customerName = session.customerName.trimmed().isEmpty()
        ? order.customerName : session.customerName.trimmed();
    order.customerContact = session.customerContact.trimmed();
    const QVector<OrderDetail> details = orderDetails(order.id, updatedItems);
    if (!updateOrder(DataPaths::orders(), DataPaths::orderDetail(), order,
            details, error))
    {
        if (order.status == QStringLiteral("paid"))
        {
            QString rollbackError;
            if (!saveProducts(DataPaths::products(), products, rollbackError))
                error += QStringLiteral(" Inventory rollback also failed: %1").arg(rollbackError);
        }
        return false;
    }

    session.items = updatedItems;
    error.clear();
    return true;
}

bool completePayment(OrderSession &session, QString paymentMode,
    QString merchant, double amountPaid, Bill &bill, QString &error)
{
    bill = Bill{};
    if (session.items.isEmpty())
    {
        error = QStringLiteral("The order is empty.");
        return false;
    }

    CheckoutRequest request;
    request.customerName = session.customerName;
    request.customerContact = session.customerContact;
    request.existingOrderId = session.orderId;
    request.paymentMode = paymentMode;
    request.merchant = merchant;
    request.amountPaid = amountPaid;
    request.items = session.items;

    CheckoutResult result;
    if (!generateBill(request, result, error))
        return false;

    bill = result.bill;
    session = OrderSession{};
    return true;
}
}
