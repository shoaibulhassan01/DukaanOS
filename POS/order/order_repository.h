#pragma once

#include "order.h"
#include "order_detail.h"

#include <QString>
#include <QVector>

namespace Pos
{
// Owner: Zain. TODO: Read Order.txt into the list.
bool readOrders(QString filePath, QVector<Order> &orders, QString &error);

// Owner: Zain. TODO: Read detail rows for one order ID.
bool readOrderDetails(QString filePath, QString orderId,
    QVector<OrderDetail> &details, QString &error);

// Owner: Zain. TODO: Save one order and its detail rows.
bool saveOrder(QString orderFile, QString detailFile, Order order,
    QVector<OrderDetail> details, QString &error);

// Owner: Zain. TODO: Change an order's status in Order.txt.
bool changeOrderStatus(QString filePath, QString orderId, QString status,
    QString &error);
}
