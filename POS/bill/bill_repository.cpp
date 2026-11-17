#include "bill_repository.h"

#include <QFile>
#include <QStringList>
#include <QTextStream>

namespace Pos
{
namespace
{
// Bill.txt line format: 12 fields separated by one space, line ends with '#'.
constexpr int kBillFieldCount = 12;

QString toField(QString text)
{
    text = text.trimmed();
    if (text.isEmpty())
        return QStringLiteral("-");
    return text.replace(' ', '_');
}

QString fromField(QString text)
{
    return text == QStringLiteral("-") ? QString() : text.replace('_', ' ');
}

QString money(double value)
{
    return QString::number(value, 'f', 2);
}

QString toLine(const Bill &b)
{
    const QStringList fields = {
        toField(b.id),
        toField(b.customerName),
        toField(b.orderId),
        toField(b.productAcronym),
        money(b.unitPrice),
        money(b.discountPerUnit),
        QString::number(b.quantity),
        money(b.totalWithoutTax),
        money(b.totalTax),
        money(b.totalDiscount),
        money(b.totalPayable),
        toField(b.paymentStatus)
    };
    return fields.join(' ') + QStringLiteral("#");
}

bool parseLine(const QString &rawLine, Bill &bill)
{
    QString line = rawLine.trimmed();
    if (!line.endsWith('#'))
        return false;
    line.chop(1);

    const QStringList f = line.split(' ', Qt::SkipEmptyParts);
    if (f.size() != kBillFieldCount)
        return false;

    bool ok[6] = {};
    Bill b;
    b.id = f[0];
    b.customerName = fromField(f[1]);
    b.orderId = f[2];
    b.productAcronym = f[3];
    b.unitPrice = f[4].toDouble(&ok[0]);
    b.discountPerUnit = f[5].toDouble(&ok[1]);
    b.quantity = f[6].toInt(&ok[2]);
    b.totalWithoutTax = f[7].toDouble(&ok[3]);
    b.totalTax = f[8].toDouble(&ok[4]);
    b.totalDiscount = f[9].toDouble(&ok[5]);
    bool okPayable = false;
    b.totalPayable = f[10].toDouble(&okPayable);
    b.paymentStatus = f[11];

    for (bool value : ok)
        if (!value)
            return false;
    if (!okPayable)
        return false;

    bill = b;
    return true;
}
}

// Reads every row of Bill.txt that has this bill ID.
bool readBill(QString filePath, QString billId, QVector<Bill> &rows,
    QString &error)
{
    rows.clear();
    error.clear();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error = QStringLiteral("Cannot open bill file: %1").arg(filePath);
        return false;
    }

    QVector<Bill> found;
    QTextStream in(&file);
    int lineNumber = 0;
    while (!in.atEnd()) {
        const QString line = in.readLine();
        ++lineNumber;
        if (line.trimmed().isEmpty())
            continue;

        Bill bill;
        if (!parseLine(line, bill)) {
            error = QStringLiteral("Bad line %1 in %2").arg(lineNumber).arg(filePath);
            return false;
        }
        if (bill.id == billId)
            found.append(bill);
    }

    if (found.isEmpty()) {
        error = QStringLiteral("Bill %1 not found.").arg(billId);
        return false;
    }

    rows = found;
    return true;
}

// Appends one line per product row to Bill.txt (creates the file if missing).
bool saveBill(QString filePath, QVector<Bill> rows, QString &error)
{
    error.clear();
    if (rows.isEmpty()) {
        error = QStringLiteral("Cannot save an empty bill.");
        return false;
    }

    // Build all lines first so a bad row never leaves a half-written bill.
    QStringList lines;
    for (const Bill &row : rows) {
        if (row.id.trimmed().isEmpty() || row.orderId.trimmed().isEmpty()
            || row.productAcronym.trimmed().isEmpty()) {
            error = QStringLiteral("Bill row is missing its bill ID, order ID or acronym.");
            return false;
        }
        lines.append(toLine(row));
    }

    // If the file does not end with a newline, add one before appending.
    bool needsNewline = false;
    {
        QFile existing(filePath);
        if (existing.exists() && existing.size() > 0
            && existing.open(QIODevice::ReadOnly)) {
            existing.seek(existing.size() - 1);
            char last = 0;
            needsNewline = existing.read(&last, 1) == 1 && last != '\n';
        }
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        error = QStringLiteral("Cannot write bill file: %1").arg(filePath);
        return false;
    }
    if (needsNewline)
        file.write("\n");

    QTextStream out(&file);
    for (const QString &line : lines)
        out << line << '\n';
    out.flush();

    if (out.status() != QTextStream::Ok) {
        error = QStringLiteral("Failed while writing %1").arg(filePath);
        return false;
    }
    return true;
}
}
