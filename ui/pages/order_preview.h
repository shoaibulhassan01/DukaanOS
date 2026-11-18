#pragma once

#include "../../POS/product/product.h"

#include <QString>
#include <QVector>

struct OrderPreviewLine
{
    Pos::Product product;
    int quantity = 0;
};

struct OrderPreview
{
    QString id;              // bill ID (9 digits) once the bill is saved
    QString orderId;         // order ID (8 digits) once the order is saved
    QString status;          // order status from Order.txt, e.g. paid
    QString customerName;
    QString phone;
    QString paymentMethod;
    QString createdAt;
    QVector<OrderPreviewLine> lines;
    double subtotal = 0;
    double tax = 0;
    double discount = 0;
    double total = 0;
    int itemCount = 0;
};
