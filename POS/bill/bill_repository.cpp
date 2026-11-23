#include "bill_repository.h"

#include <cmath>
#include <QFile>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>

namespace Pos
{
namespace
{
// Legacy rows have 12 fields; current rows also store discounts and settlement.
constexpr int kLegacyBillFieldCount = 12;
constexpr int kBillFieldCount = 16;

bool isSafeToken(const QString &text)
{
    if (text.isEmpty() || text.contains(QLatin1Char('#')))
        return false;
    for (const QChar character : text) {
        if (character.isSpace())
            return false;
    }

    return true;
}

bool isValidMoney(double value)
{
    return std::isfinite(value) && value >= 0;
}

double round2(double value)
{
    return std::round(value * 100.0) / 100.0;
}

QString toField(QString text)
{
    text = text.trimmed();
    if (text.isEmpty())
        return QStringLiteral("-");
    return text.replace(QRegularExpression(QStringLiteral("\\s+")),
        QStringLiteral("_"));
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
        toField(b.paymentStatus),
        money(b.seasonalDiscount),
        money(b.specialDiscount),
        money(b.amountReceived),
        money(b.balanceReturned)
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
    if (f.size() != kLegacyBillFieldCount && f.size() != kBillFieldCount)
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
    bool okSeasonal = true;
    bool okSpecial = true;
    bool okReceived = true;
    bool okBalance = true;
    if (f.size() == kBillFieldCount) {
        b.seasonalDiscount = f[12].toDouble(&okSeasonal);
        b.specialDiscount = f[13].toDouble(&okSpecial);
        b.amountReceived = f[14].toDouble(&okReceived);
        b.balanceReturned = f[15].toDouble(&okBalance);
    }

    for (bool value : ok)
        if (!value)
            return false;
    if (!okPayable || !okSeasonal || !okSpecial || !okReceived || !okBalance)
        return false;
    if (!isSafeToken(b.id) || !isSafeToken(b.orderId)
        || !isSafeToken(b.productAcronym) || !isSafeToken(b.paymentStatus)
        || b.quantity <= 0 || !isValidMoney(b.unitPrice)
        || !isValidMoney(b.discountPerUnit) || !isValidMoney(b.totalWithoutTax)
        || !isValidMoney(b.totalTax) || !isValidMoney(b.totalDiscount)
        || !isValidMoney(b.seasonalDiscount)
        || !isValidMoney(b.specialDiscount)
        || !isValidMoney(b.totalPayable)
        || b.totalWithoutTax != round2(b.unitPrice * b.quantity)
        || b.totalDiscount != round2(b.discountPerUnit * b.quantity)
        || b.totalPayable != round2(b.totalWithoutTax + b.totalTax
            - b.totalDiscount - b.seasonalDiscount - b.specialDiscount)
        || b.balanceReturned > b.amountReceived) {
        return false;
    }

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

bool readAllBills(QString filePath, QVector<Bill> &rows, QString &error)
{
    rows.clear();
    error.clear();
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error = file.exists()
            ? QStringLiteral("Cannot open bill file: %1").arg(filePath)
            : QStringLiteral("Bill file does not exist: %1").arg(filePath);
        return false;
    }

    QVector<Bill> loaded;
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
        loaded.append(bill);
    }
    rows = loaded;
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

    const Bill &first = rows.first();
    QStringList lines;
    for (const Bill &row : rows) {
        if (!isSafeToken(row.id) || !isSafeToken(row.orderId)
            || !isSafeToken(row.productAcronym) || !isSafeToken(row.paymentStatus)
            || row.customerName.contains(QLatin1Char('#')) || row.quantity <= 0) {
            error = QStringLiteral("Bill row is missing a required field or has an invalid quantity.");
            return false;
        }
        if (row.id != first.id || row.orderId != first.orderId
            || row.customerName != first.customerName
            || row.paymentStatus != first.paymentStatus
            || row.amountReceived != first.amountReceived
            || row.balanceReturned != first.balanceReturned) {
            error = QStringLiteral("All bill rows must share the same bill, order, customer and status.");
            return false;
        }
        if (!isValidMoney(row.unitPrice) || !isValidMoney(row.discountPerUnit)
            || !isValidMoney(row.totalWithoutTax) || !isValidMoney(row.totalTax)
            || !isValidMoney(row.totalDiscount) || !isValidMoney(row.totalPayable)
            || row.totalWithoutTax != round2(row.unitPrice * row.quantity)
            || !isValidMoney(row.seasonalDiscount)
            || !isValidMoney(row.specialDiscount)
            || !isValidMoney(row.amountReceived)
            || !isValidMoney(row.balanceReturned)
            || row.totalDiscount != round2(row.discountPerUnit * row.quantity)
            || row.totalPayable != round2(row.totalWithoutTax + row.totalTax
                - row.totalDiscount - row.seasonalDiscount - row.specialDiscount)) {
            error = QStringLiteral("Bill row contains invalid or inconsistent amounts.");
            return false;
        }
        lines.append(toLine(row));
    }

    double billPayable = 0;
    for (const Bill &row : rows)
        billPayable += row.totalPayable;
    billPayable = round2(billPayable);
    if (first.balanceReturned > first.amountReceived
        || round2(first.amountReceived - first.balanceReturned) != billPayable) {
        error = QStringLiteral("Received amount and returned balance must match the bill total.");
        return false;
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
