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
    QString id;
    QString customerName;
    QString phone;
    QString paymentMethod;
    QString merchant;
    QString createdAt;
    QVector<OrderPreviewLine> lines;
    double subtotal = 0;
    double tax = 0;
    double discount = 0;
    double seasonalDiscount = 0;
    double specialDiscount = 0;
    double specialDiscountPercent = 0;
    double amountReceived = 0;
    double balanceReturned = 0;
    double total = 0;
    int itemCount = 0;
};
