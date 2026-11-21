#pragma once

#include "bill.h"

#include <QString>
#include <QVector>

namespace Pos
{
// Reads one Bill.txt header and its BillDetail.txt rows.
bool readBill(QString billFile, QString detailFile, QString billId, Bill &bill,
    QString &error);

// Reads the bill header and details belonging to an order.
bool readBillForOrder(QString billFile, QString detailFile, QString orderId,
    Bill &bill, QString &error);

// Appends one header to Bill.txt and one row per item to BillDetail.txt.
bool saveBill(QString billFile, QString detailFile, const Bill &bill,
    QString &error);
}
