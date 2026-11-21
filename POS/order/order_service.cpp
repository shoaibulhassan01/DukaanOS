#include "order_service.h"

#include "../checkout/checkout_service.h"
#include "../common/data_paths.h"
#include "../common/date_format.h"
#include "../common/id_generator.h"
#include "../order/order_repository.h"
#include "../product/product_repository.h"

#include <QDateTime>

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

bool resumeOrder(QString orderId, OrderSession &session, QString &error)
{
    error.clear();
    Order order;
    if (!findOrder(orderId, order, error))
        return false;
    if (order.status != QStringLiteral("on-hold"))
    {
        error = QStringLiteral("Order %1 is not on hold.").arg(orderId);
        return false;
    }

    QVector<OrderDetail> details;
    if (!readOrderDetails(DataPaths::orderDetail(), orderId, details, error))
        return false;
    if (details.isEmpty())
    {
        error = QStringLiteral("Held order %1 has no saved products.").arg(orderId);
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
            error = QStringLiteral(
                "Product %1 from held order %2 is no longer available.")
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
