#include "bill_repository.h"

#include "../common/record_codec.h"

#include <cmath>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTime>
#include <QTextStream>

namespace Pos
{
namespace
{
constexpr int kBillFieldCount = 14;
constexpr int kBillDetailFieldCount = 5;

bool safeToken(const QString &value)
{
    if (value.isEmpty() || value.contains(QLatin1Char('#')))
        return false;
    for (const QChar ch : value)
    {
        if (ch.isSpace())
            return false;
    }
    return true;
}

bool validMoney(double value)
{
    return std::isfinite(value) && value >= 0;
}

double round2(double value)
{
    return std::round(value * 100.0) / 100.0;
}

QString money(double value)
{
    return QString::number(value, 'f', 2);
}

QString toField(QString value)
{
    value = value.trimmed();
    if (value.isEmpty())
        return QStringLiteral("-");
    return value.replace(QRegularExpression(QStringLiteral("\\s+")),
        QStringLiteral("_"));
}

QString fromField(QString value)
{
    return value == QStringLiteral("-")
        ? QString() : value.replace(QLatin1Char('_'), QLatin1Char(' '));
}

QString billLine(const Bill &bill)
{
    return makeRecord({
        bill.id,
        toField(bill.customerName),
        bill.orderId,
        money(bill.totalWithoutTax),
        money(bill.totalTax),
        money(bill.totalDiscount),
        money(bill.totalPayable),
        money(bill.amountReceived),
        money(bill.balanceReturned),
        bill.paymentStatus,
        money(bill.seasonalDiscount),
        money(bill.specialDiscount),
        bill.date,
        bill.time
    });
}

QString detailLine(const BillDetail &detail)
{
    return makeRecord({
        detail.billId,
        detail.productAcronym,
        money(detail.unitPrice),
        QString::number(detail.quantity),
        money(detail.totalDiscount)
    });
}

bool parseBillLine(const QString &line, Bill &bill)
{
    QStringList fields;
    if (!splitRecord(line, fields) || fields.size() != kBillFieldCount)
        return false;

    static const QRegularExpression billIdPattern(QStringLiteral("^\\d{9}$"));
    bool ok[8] = {};
    Bill parsed;
    parsed.id = fields.at(0);
    parsed.customerName = fromField(fields.at(1));
    parsed.orderId = fields.at(2);
    parsed.totalWithoutTax = fields.at(3).toDouble(&ok[0]);
    parsed.totalTax = fields.at(4).toDouble(&ok[1]);
    parsed.totalDiscount = fields.at(5).toDouble(&ok[2]);
    parsed.totalPayable = fields.at(6).toDouble(&ok[3]);
    parsed.amountReceived = fields.at(7).toDouble(&ok[4]);
    parsed.balanceReturned = fields.at(8).toDouble(&ok[5]);
    parsed.paymentStatus = fields.at(9);
    parsed.seasonalDiscount = fields.at(10).toDouble(&ok[6]);
    parsed.specialDiscount = fields.at(11).toDouble(&ok[7]);
    parsed.date = fields.at(12);
    parsed.time = fields.at(13);

    for (bool valid : ok)
    {
        if (!valid)
            return false;
    }
    if (!billIdPattern.match(parsed.id).hasMatch()
        || !safeToken(parsed.orderId)
        || !safeToken(parsed.paymentStatus)
        || !QDate::fromString(parsed.date, Qt::ISODate).isValid()
        || !QTime::fromString(parsed.time, QStringLiteral("HH:mm:ss")).isValid()
        || !validMoney(parsed.totalWithoutTax)
        || !validMoney(parsed.totalTax)
        || !validMoney(parsed.totalDiscount)
        || !validMoney(parsed.totalPayable)
        || !validMoney(parsed.amountReceived)
        || !validMoney(parsed.balanceReturned)
        || !validMoney(parsed.seasonalDiscount)
        || !validMoney(parsed.specialDiscount)
        || parsed.specialDiscount
            > (parsed.totalWithoutTax + parsed.totalTax) * 0.08 + 0.005
        || round2(parsed.totalWithoutTax + parsed.totalTax - parsed.totalDiscount
            - parsed.seasonalDiscount - parsed.specialDiscount)
            != parsed.totalPayable
        || parsed.balanceReturned > parsed.amountReceived
        || (parsed.paymentStatus == QStringLiteral("paid")
            && round2(parsed.amountReceived - parsed.balanceReturned)
                != parsed.totalPayable))
    {
        return false;
    }
    bill = parsed;
    return true;
}

bool parseDetailLine(const QString &line, BillDetail &detail)
{
    QStringList fields;
    if (!splitRecord(line, fields) || fields.size() != kBillDetailFieldCount)
        return false;

    bool priceOk = false;
    bool quantityOk = false;
    bool discountOk = false;
    BillDetail parsed;
    parsed.billId = fields.at(0);
    parsed.productAcronym = fields.at(1);
    parsed.unitPrice = fields.at(2).toDouble(&priceOk);
    parsed.quantity = fields.at(3).toInt(&quantityOk);
    parsed.totalDiscount = fields.at(4).toDouble(&discountOk);
    if (!QRegularExpression(QStringLiteral("^\\d{9}$"))
            .match(parsed.billId).hasMatch()
        || !safeToken(parsed.productAcronym)
        || !priceOk || !quantityOk || !discountOk
        || parsed.quantity <= 0
        || !validMoney(parsed.unitPrice)
        || !validMoney(parsed.totalDiscount)
        || parsed.totalDiscount > round2(parsed.unitPrice * parsed.quantity))
    {
        return false;
    }
    detail = parsed;
    return true;
}

bool readBillRecords(const QString &path, QVector<Bill> &bills, QString &error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        error = QStringLiteral("Cannot open bill file: %1").arg(path);
        return false;
    }
    QVector<Bill> loaded;
    QSet<QString> ids;
    QTextStream in(&file);
    int lineNumber = 0;
    while (!in.atEnd())
    {
        const QString raw = in.readLine();
        ++lineNumber;
        if (raw.trimmed().isEmpty())
            continue;
        Bill bill;
        if (!parseBillLine(raw, bill) || ids.contains(bill.id))
        {
            error = QStringLiteral("Invalid or duplicate bill at line %1 in %2.")
                .arg(lineNumber).arg(path);
            return false;
        }
        ids.insert(bill.id);
        loaded.append(bill);
    }
    bills = loaded;
    return true;
}

bool readBillDetails(const QString &path, QVector<BillDetail> &details,
    QString &error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        error = QStringLiteral("Cannot open bill detail file: %1").arg(path);
        return false;
    }
    QVector<BillDetail> loaded;
    QTextStream in(&file);
    int lineNumber = 0;
    while (!in.atEnd())
    {
        const QString raw = in.readLine();
        ++lineNumber;
        if (raw.trimmed().isEmpty())
            continue;
        BillDetail detail;
        if (!parseDetailLine(raw, detail))
        {
            error = QStringLiteral("Invalid bill detail at line %1 in %2.")
                .arg(lineNumber).arg(path);
            return false;
        }
        loaded.append(detail);
    }
    details = loaded;
    return true;
}

bool validateDetails(const Bill &bill, QString &error)
{
    if (bill.details.isEmpty())
    {
        error = QStringLiteral("A bill must contain at least one product.");
        return false;
    }
    double subtotal = 0;
    double discount = 0;
    for (const BillDetail &detail : bill.details)
    {
        if (detail.billId != bill.id || !safeToken(detail.productAcronym)
            || detail.quantity <= 0 || !validMoney(detail.unitPrice)
            || !validMoney(detail.totalDiscount)
            || detail.totalDiscount > round2(detail.unitPrice * detail.quantity))
        {
            error = QStringLiteral("Bill detail is invalid or belongs to another bill.");
            return false;
        }
        subtotal += round2(detail.unitPrice * detail.quantity);
        discount += detail.totalDiscount;
    }
    if (round2(subtotal) != bill.totalWithoutTax
        || round2(discount) != bill.totalDiscount)
    {
        error = QStringLiteral("Bill totals do not match its product details.");
        return false;
    }
    return true;
}

bool validateBill(const Bill &bill, QString &error)
{
    if (!QRegularExpression(QStringLiteral("^\\d{9}$")).match(bill.id).hasMatch()
        || !safeToken(bill.orderId)
        || !safeToken(bill.paymentStatus)
        || bill.customerName.trimmed().isEmpty()
        || bill.customerName.contains(QLatin1Char('#'))
        || !QDate::fromString(bill.date, Qt::ISODate).isValid()
        || !QTime::fromString(bill.time, QStringLiteral("HH:mm:ss")).isValid()
        || !validMoney(bill.totalWithoutTax)
        || !validMoney(bill.totalTax)
        || !validMoney(bill.totalDiscount)
        || !validMoney(bill.totalPayable)
        || !validMoney(bill.amountReceived)
        || !validMoney(bill.balanceReturned)
        || !validMoney(bill.seasonalDiscount)
        || !validMoney(bill.specialDiscount)
        || bill.specialDiscount
            > (bill.totalWithoutTax + bill.totalTax) * 0.08 + 0.005
        || round2(bill.totalWithoutTax + bill.totalTax - bill.totalDiscount
            - bill.seasonalDiscount - bill.specialDiscount)
            != bill.totalPayable
        || bill.balanceReturned > bill.amountReceived
        || (bill.paymentStatus == QStringLiteral("paid")
            && round2(bill.amountReceived - bill.balanceReturned)
                != bill.totalPayable))
    {
        error = QStringLiteral("Bill header contains invalid or inconsistent fields.");
        return false;
    }
    return validateDetails(bill, error);
}

bool appendLines(const QString &path, const QStringList &lines, QString &error)
{
    bool needsNewline = false;
    QFile existing(path);
    if (existing.exists() && existing.size() > 0)
    {
        if (!existing.open(QIODevice::ReadOnly))
        {
            error = QStringLiteral("Cannot read %1: %2")
                .arg(path, existing.errorString());
            return false;
        }
        existing.seek(existing.size() - 1);
        char last = 0;
        needsNewline = existing.read(&last, 1) == 1 && last != '\n';
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    {
        error = QStringLiteral("Cannot write %1: %2")
            .arg(path, file.errorString());
        return false;
    }
    QTextStream out(&file);
    if (needsNewline)
        out << '\n';
    for (const QString &line : lines)
        out << line << '\n';
    out.flush();
    if (out.status() != QTextStream::Ok || !file.flush())
    {
        error = QStringLiteral("Failed while writing %1.").arg(path);
        return false;
    }
    return true;
}

// Reads a data file as lines (without line endings); a missing file is empty.
bool readLines(const QString &path, QStringList &lines, QString &error)
{
    lines.clear();
    QFile file(path);
    if (!file.exists())
        return true;
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        error = QStringLiteral("Cannot read %1: %2").arg(path, file.errorString());
        return false;
    }
    QTextStream in(&file);
    while (!in.atEnd())
        lines.append(in.readLine());
    return true;
}

// Writes the whole file at once (a failed write leaves the old file).
bool writeLines(const QString &path, const QStringList &lines, QString &error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        error = QStringLiteral("Cannot write %1: %2").arg(path, file.errorString());
        return false;
    }
    QTextStream out(&file);
    for (const QString &line : lines)
        out << line << '\n';
    out.flush();
    if (out.status() != QTextStream::Ok || !file.commit())
    {
        error = QStringLiteral("Failed while writing %1.").arg(path);
        return false;
    }
    return true;
}

QString firstField(const QString &line)
{
    return line.trimmed().section(QLatin1Char(' '), 0, 0);
}

bool attachDetails(const QString &detailFile, Bill &bill, QString &error)
{
    QVector<BillDetail> allDetails;
    if (!readBillDetails(detailFile, allDetails, error))
        return false;
    for (const BillDetail &detail : allDetails)
    {
        if (detail.billId == bill.id)
            bill.details.append(detail);
    }
    return validateDetails(bill, error);
}
}

bool readBill(QString billFile, QString detailFile, QString billId, Bill &bill,
    QString &error)
{
    bill = Bill{};
    QVector<Bill> allBills;
    if (!readBillRecords(billFile, allBills, error))
        return false;
    for (Bill &candidate : allBills)
    {
        if (candidate.id != billId)
            continue;
        if (!attachDetails(detailFile, candidate, error))
            return false;
        bill = candidate;
        error.clear();
        return true;
    }
    error = QStringLiteral("Bill %1 not found.").arg(billId);
    return false;
}

bool readBills(QString billFile, QVector<Bill> &bills, QString &error)
{
    bills.clear();
    if (!QFile::exists(billFile))
        return true;
    return readBillRecords(billFile, bills, error);
}

bool readBillForOrder(QString billFile, QString detailFile, QString orderId,
    Bill &bill, QString &error)
{
    bill = Bill{};
    QVector<Bill> allBills;
    if (!readBillRecords(billFile, allBills, error))
        return false;
    bool found = false;
    for (Bill &candidate : allBills)
    {
        if (candidate.orderId != orderId)
            continue;
        if (found)
        {
            error = QStringLiteral("More than one bill exists for order %1.")
                .arg(orderId);
            return false;
        }
        if (!attachDetails(detailFile, candidate, error))
            return false;
        bill = candidate;
        found = true;
    }
    if (!found)
    {
        error = QStringLiteral("No bill is saved for order %1.").arg(orderId);
        return false;
    }
    error.clear();
    return true;
}

bool saveBill(QString billFile, QString detailFile, const Bill &bill,
    QString &error)
{
    error.clear();
    if (!validateBill(bill, error))
        return false;

    QLockFile lock(billFile + QStringLiteral(".lock"));
    if (!lock.tryLock(0))
    {
        error = QStringLiteral("Another checkout is saving a bill. Try again.");
        return false;
    }

    QVector<Bill> existing;
    if (QFile::exists(billFile) && !readBillRecords(billFile, existing, error))
        return false;
    for (const Bill &saved : existing)
    {
        if (saved.id == bill.id)
        {
            error = QStringLiteral("Bill ID %1 already exists.").arg(bill.id);
            return false;
        }
    }

    QVector<BillDetail> existingDetails;
    if (QFile::exists(detailFile)
        && !readBillDetails(detailFile, existingDetails, error))
        return false;
    for (const BillDetail &saved : existingDetails)
    {
        if (saved.billId == bill.id)
        {
            error = QStringLiteral(
                "Bill ID %1 already exists in BillDetail.txt.").arg(bill.id);
            return false;
        }
    }

    QStringList detailLines;
    for (const BillDetail &detail : bill.details)
        detailLines.append(detailLine(detail));

    // The header is the commit marker: a failed header append leaves no
    // visible bill, and the orphan detail ID prevents that ID being reused.
    if (!appendLines(detailFile, detailLines, error))
        return false;
    if (!appendLines(billFile, {billLine(bill)}, error))
        return false;
    error.clear();
    return true;
}

bool updateBill(QString billFile, QString detailFile, const Bill &bill,
    QString &error)
{
    error.clear();
    if (!validateBill(bill, error))
        return false;

    QLockFile lock(billFile + QStringLiteral(".lock"));
    if (!lock.tryLock(0))
    {
        error = QStringLiteral("Another checkout is saving a bill. Try again.");
        return false;
    }

    QStringList billLines;
    if (!readLines(billFile, billLines, error))
        return false;
    int headerRow = -1;
    for (int i = 0; i < billLines.size(); ++i)
    {
        if (firstField(billLines.at(i)) == bill.id)
        {
            headerRow = i;
            break;
        }
    }
    if (headerRow < 0)
    {
        error = QStringLiteral("Bill %1 not found in Bill.txt.").arg(bill.id);
        return false;
    }

    // The new detail rows go where the old ones were, so the file keeps
    // the bills in the order they were made.
    QStringList oldDetails;
    if (!readLines(detailFile, oldDetails, error))
        return false;
    QStringList newDetails;
    bool inserted = false;
    for (const QString &line : oldDetails)
    {
        if (firstField(line) != bill.id)
        {
            newDetails.append(line);
            continue;
        }
        if (!inserted)
        {
            for (const BillDetail &detail : bill.details)
                newDetails.append(detailLine(detail));
            inserted = true;
        }
    }
    if (!inserted)
        for (const BillDetail &detail : bill.details)
            newDetails.append(detailLine(detail));

    billLines[headerRow] = billLine(bill);
    if (!writeLines(detailFile, newDetails, error))
        return false;
    if (!writeLines(billFile, billLines, error))
    {
        QString restoreError;
        if (!writeLines(detailFile, oldDetails, restoreError))
            error += QStringLiteral(" BillDetail.txt could not be restored: %1").arg(restoreError);
        return false;
    }
    error.clear();
    return true;
}
}
