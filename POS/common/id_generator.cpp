
#include "id_generator.h"
#include "record_codec.h"

#include <QDate>
#include <QFile>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>

namespace Pos
{
namespace
{
bool nextId(const QString &filePath, const QDate &date, bool billId,
    QString &id, QString &error)
{
    id.clear();
    error.clear();
    if (!date.isValid())
    {
        error = QStringLiteral("Cannot generate an ID for an invalid date.");
        return false;
    }

    const int width = billId ? 9 : 8;
    const int sequenceWidth = billId ? 3 : 2;
    const int maximum = billId ? 999 : 99;
    const QString prefix = date.toString(QStringLiteral("yyMMdd"));
    int highest = 0;
    QFile file(filePath);
    if (file.exists() && !file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        error = QStringLiteral("Cannot read %1: %2").arg(filePath, file.errorString());
        return false;
    }
    if (file.isOpen())
    {
        QTextStream in(&file);
        while (!in.atEnd())
        {
            QStringList fields;
            const QString line = in.readLine().trimmed();
            if (line.isEmpty())
                continue;
            const bool validCount = splitRecord(line, fields)
                && (billId ? (fields.size() == 12 || fields.size() == 16)
                           : fields.size() == 9);
            if (!validCount)
            {
                error = QStringLiteral("Invalid record in %1.").arg(filePath);
                return false;
            }
            const QString existing = fields.first();
            if (!QRegularExpression(QStringLiteral("^\\d{%1}$").arg(width))
                    .match(existing).hasMatch())
            {
                error = QStringLiteral("Invalid ID in %1.").arg(filePath);
                return false;
            }
            if (existing.left(6) == prefix)
                highest = qMax(highest, existing.right(sequenceWidth).toInt());
        }
    }
    if (highest >= maximum)
    {
        error = QStringLiteral("Daily ID limit reached for %1.").arg(filePath);
        return false;
    }
    id = prefix + QStringLiteral("%1")
        .arg(highest + 1, sequenceWidth, 10, QLatin1Char('0'));
    return true;
}
}

bool generateOrderId(QString &id)
{
    QString error;
    return nextId(QStringLiteral("Order.txt"), QDate::currentDate(),
        false, id, error);
}

bool generateBillId(QString &id)
{
    QString error;
    return nextId(QStringLiteral("Bill.txt"), QDate::currentDate(),
        true, id, error);
}

bool generateOrderId(QString filePath, QDate date, QString &id, QString &error)
{
    return nextId(filePath, date, false, id, error);
}

bool generateBillId(QString filePath, QDate date, QString &id, QString &error)
{
    return nextId(filePath, date, true, id, error);
}
}
