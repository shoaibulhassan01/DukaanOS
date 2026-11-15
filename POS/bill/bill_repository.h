#pragma once

#include "bill.h"

#include <QString>
#include <QVector>

namespace Pos
{
// Owner: Ahad. TODO: Read bill rows with this bill ID from Bill.txt.
bool readBill(QString filePath, QString billId, QVector<Bill> &rows,
    QString &error);

// Owner: Ahad. TODO: Save each product row of the bill to Bill.txt.
bool saveBill(QString filePath, QVector<Bill> rows, QString &error);
}
