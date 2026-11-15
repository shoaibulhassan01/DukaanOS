#include "bill_repository.h"

namespace Pos
{
// TODO: Read bill rows with this bill ID from Bill.txt.
bool readBill(QString filePath, QString billId, QVector<Bill> &rows,
    QString &error)
{
    error = QStringLiteral("TODO: Read bill rows from Bill.txt.");
    return false;
}

// TODO: Save each product row of the bill to Bill.txt.
bool saveBill(QString filePath, QVector<Bill> rows, QString &error)
{
    error = QStringLiteral("TODO: Save bill rows to Bill.txt.");
    return false;
}
}
