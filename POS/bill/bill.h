#pragma once

#include <QString>
#include <QVector>

namespace Pos
{
struct BillDetail
{
    QString billId;
    QString productAcronym;
    double unitPrice = 0;
    int quantity = 0;
    double totalDiscount = 0;
};

struct BillLine
{
    BillDetail detail;
    double totalWithoutTax = 0;
    double totalTax = 0;
    double totalPayable = 0;
};

struct Bill
{
    QString id;
    QString customerName;
    QString orderId;
    double totalWithoutTax = 0;
    double totalTax = 0;
    double totalDiscount = 0;
    double totalPayable = 0;
    double amountReceived = 0;
    double balanceReturned = 0;
    QString paymentStatus;
    double seasonalDiscount = 0;
    double specialDiscount = 0;
    QString date;
    QString time;
    QVector<BillDetail> details;
};
}
