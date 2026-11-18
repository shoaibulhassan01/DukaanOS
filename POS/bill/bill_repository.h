#pragma once

#include "bill.h"

#include <QString>
#include <QVector>

namespace Pos
{
// Owner: Abdul Ahad - read bill rows with this bill ID from Bill.txt.
bool readBill(QString filePath, QString billId, QVector<Bill> &rows,
    QString &error);

// Owner: Abdul Ahad - read the bill rows that belong to one order (used to
// show an old bill from Order History).
bool readBillForOrder(QString filePath, QString orderId, QVector<Bill> &rows,
    QString &error);

// Owner: Abdul Ahad - save each product row of the bill to Bill.txt.
bool saveBill(QString filePath, QVector<Bill> rows, QString &error);
}
