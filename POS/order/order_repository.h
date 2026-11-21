#pragma once

#include "order.h"
#include "order_detail.h"

#include <QString>
#include <QVector>

namespace Pos
{
// Owner: Zain Ul Abideen - read Order.txt into the list.
bool readOrders(QString filePath, QVector<Order> &orders, QString &error);

// Owner: Zain Ul Abideen - read detail rows for one order ID.
bool readOrderDetails(QString filePath, QString orderId,
    QVector<OrderDetail> &details, QString &error);

// Owner: Zain Ul Abideen - save one order and its detail rows.
bool saveOrder(QString orderFile, QString detailFile, Order order,
    QVector<OrderDetail> details, QString &error);

// Owner: Ahad. Replace an existing order and its product rows without adding
// a duplicate order ID (used when a held order is resumed and checked out).
bool updateOrder(QString orderFile, QString detailFile, Order order,
    QVector<OrderDetail> details, QString &error);

// Owner: Zain Ul Abideen - change an order's status in Order.txt.
bool changeOrderStatus(QString filePath, QString orderId, QString status,
    QString &error);
}
