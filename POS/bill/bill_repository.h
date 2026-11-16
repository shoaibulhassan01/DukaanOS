#pragma once

#include "bill.h"

#include <QString>
#include <QVector>

namespace Pos
{
// Owner: Abdul Ahad - read bill rows with this bill ID from Bill.txt.
bool readBill(QString filePath, QString billId, QVector<Bill> &rows,
    QString &error);

// Owner: Abdul Ahad - save each product row of the bill to Bill.txt.
bool saveBill(QString filePath, QVector<Bill> rows, QString &error);
}
