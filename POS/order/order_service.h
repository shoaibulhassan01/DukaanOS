#pragma once

#include "../bill/bill.h"
#include "cart_item.h"
#include "order.h"

#include <QString>
#include <QVector>

namespace Pos
{
struct OrderSession
{
    QString orderId;
    QString customerName = QStringLiteral("Walk-in");
    QString customerContact;
    QVector<CartItem> items;
};

bool readHeldOrders(QVector<Order> &orders, QString &error);

bool addProductToOrder(OrderSession &session, QString barcode, int quantity,
    QString &error);
bool removeProductFromOrder(OrderSession &session, QString productId,
    QString &error);

bool holdOrder(OrderSession &session, QString &error);

bool resumeOrder(QString orderId, OrderSession &session, QString &error);

bool completePayment(OrderSession &session, QString paymentMode,
    QString merchant, double amountPaid, Bill &bill, QString &error);
}
