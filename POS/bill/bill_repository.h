#pragma once

#include "bill.h"

#include <QString>
#include <QVector>

namespace Pos
{
// Reads one Bill.txt header and its BillDetail.txt rows.
bool readBill(QString billFile, QString detailFile, QString billId, Bill &bill,
    QString &error);

// Reads every Bill.txt header (no details), in file order. A missing file
// means no bills yet.
bool readBills(QString billFile, QVector<Bill> &bills, QString &error);

// Reads the bill header and details belonging to an order.
bool readBillForOrder(QString billFile, QString detailFile, QString orderId,
    Bill &bill, QString &error);

// Appends one header to Bill.txt and one row per item to BillDetail.txt.
bool saveBill(QString billFile, QString detailFile, const Bill &bill,
    QString &error);

// Replaces a saved bill (same Bill Id) after its order was edited: its line
// in Bill.txt is rewritten in place and its BillDetail.txt rows are replaced
// by the bill's new details. Every other bill is left exactly as it was.
bool updateBill(QString billFile, QString detailFile, const Bill &bill,
    QString &error);
}
