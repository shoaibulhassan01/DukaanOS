#include "order_repository.h"

namespace Pos
{
// TODO: Read Order.txt into the list.
bool readOrders(QString filePath, QVector<Order> &orders, QString &error)
{
    error = QStringLiteral("TODO: Read orders from Order.txt.");
    return false;
}

// TODO: Read detail rows for one order ID.
bool readOrderDetails(QString filePath, QString orderId,
    QVector<OrderDetail> &details, QString &error)
{
    error = QStringLiteral("TODO: Read order details from OrderDetail.txt.");
    return false;
}

// TODO: Save one order and its detail rows.
bool saveOrder(QString orderFile, QString detailFile, Order order,
    QVector<OrderDetail> details, QString &error)
{
    error = QStringLiteral("TODO: Save order and order details.");
    return false;
}

// TODO: Change an order's status in Order.txt.
bool changeOrderStatus(QString filePath, QString orderId, QString status,
    QString &error)
{
    error = QStringLiteral("TODO: Update the order status.");
    return false;
}
}
