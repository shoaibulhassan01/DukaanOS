#include "id_generator.h"
#include "record_codec.h"

#include <QFile>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>

namespace Pos
{
namespace
{
// Reads the first field (the ID) of every record in a file.
// A missing file is not an error: it just has no IDs yet.
bool readIds(const QString &filePath, int fieldCount, int idLength,
    QStringList &ids, QString &error)
{
    ids.clear();
    QFile file(filePath);
    if (!file.exists())
        return true;
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        error = QStringLiteral("Cannot open %1: %2").arg(filePath, file.errorString());
        return false;
    }

    const QRegularExpression digits(QStringLiteral("^\\d{%1}$").arg(idLength));
    QTextStream input(&file);
    int lineNumber = 0;
    while (!input.atEnd())
    {
        const QString line = input.readLine().trimmed();
        ++lineNumber;
        if (line.isEmpty())
            continue;
        QStringList fields;
        if (!splitRecord(line, fields) || fields.size() != fieldCount ||
            !digits.match(fields.at(0)).hasMatch())
        {
            error = QStringLiteral("%1 line %2 is not a valid record.").arg(filePath).arg(lineNumber);
            return false;
        }
        ids.append(fields.at(0));
    }
    return true;
}
}

// Owner: Zain Ul Abideen - running number across all orders, day at the end.
bool generateOrderId(const QString &orderFile, const QDate &date, QString &id,
    QString &error)
{
    id.clear();
    if (!date.isValid())
    {
        error = QStringLiteral("Cannot make an order ID without a valid date.");
        return false;
    }

    QStringList ids;
    if (!readIds(orderFile, 9, 8, ids, error))
        return false;

    int highest = 0;
    for (const QString &existing : ids)
        highest = qMax(highest, existing.left(6).toInt());
    if (highest >= 999999)
    {
        error = QStringLiteral("No more order IDs are available.");
        return false;
    }

    id = QStringLiteral("%1%2").arg(highest + 1, 6, 10, QLatin1Char('0'))
                               .arg(date.day(), 2, 10, QLatin1Char('0'));
    error.clear();
    return true;
}

// Owner: Zain Ul Abideen - running number per day after a yyMMdd prefix.
bool generateBillId(const QString &billFile, const QDate &date, QString &id,
    QString &error)
{
    id.clear();
    if (!date.isValid())
    {
        error = QStringLiteral("Cannot make a bill ID without a valid date.");
        return false;
    }

    QStringList ids;
    if (!readIds(billFile, 12, 9, ids, error))
        return false;

    const QString prefix = date.toString(QStringLiteral("yyMMdd"));
    int highest = 0;
    for (const QString &existing : ids)
    {
        if (existing.startsWith(prefix))
            highest = qMax(highest, existing.right(3).toInt());
    }
    if (highest >= 999)
    {
        error = QStringLiteral("No more bill IDs are available for today.");
        return false;
    }

    id = prefix + QStringLiteral("%1").arg(highest + 1, 3, 10, QLatin1Char('0'));
    error.clear();
    return true;
}
}
