#include "bill_repository.h"

namespace Pos
{
// Owner: Abdul Ahad - read bill rows with this bill ID from Bill.txt.
bool readBill(QString filePath, QString billId, QVector<Bill> &rows,
    QString &error)
{
    error = QStringLiteral("TODO: Read bill rows from Bill.txt.");
    return false;
}

// Owner: Abdul Ahad - save each product row of the bill to Bill.txt.
bool saveBill(QString filePath, QVector<Bill> rows, QString &error)
{
    error = QStringLiteral("TODO: Save bill rows to Bill.txt.");
    return false;
}
}
