
#include "id_generator.h"
#include "record_codec.h"

#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <QRegularExpression>
#include <QDate>

namespace Pos
{
    // Owner: Zain Ul Abideen - make a unique 8-digit order ID.
    bool generateOrderId(QString& id)
    {
        QFile file("Order.txt");

        QString datePrefix = QDate::currentDate().toString("yyMMdd");

        int highestNumber = 0;

        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QTextStream input(&file);

            while (!input.atEnd())
            {
                QString line = input.readLine().trimmed();

                if (line.isEmpty())
                    continue;

                QStringList fields;

                if (!splitRecord(line, fields) || fields.size() != 9)
                    return false;

                QString existingId = fields.at(0);

                if (!QRegularExpression("^\\d{8}$")
                    .match(existingId).hasMatch())
                {
                    return false;
                }

                if (existingId.left(6) == datePrefix)
                {
                    int number = existingId.right(2).toInt();

                    if (number > highestNumber)
                        highestNumber = number;
                }
            }
        }
        else if (file.exists())
        {
            return false;
        }

        if (highestNumber >= 99)
            return false;

        highestNumber++;

        QString sequence =
            QString("%1").arg(highestNumber, 2, 10, QChar('0'));

        id = datePrefix + sequence;

        return true;
    }

    // Owner: Zain Ul Abideen - make a unique 9-digit bill ID.
    bool generateBillId(QString& id)
    {
        QFile file("Bill.txt");

        QString datePrefix = QDate::currentDate().toString("yyMMdd");

        int highestNumber = 0;

        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QTextStream input(&file);

            while (!input.atEnd())
            {
                QString line = input.readLine().trimmed();

                if (line.isEmpty())
                    continue;

                QStringList fields;

                if (!splitRecord(line, fields) || fields.size() != 12)
                    return false;

                QString existingId = fields.at(0);

                if (!QRegularExpression("^\\d{9}$")
                    .match(existingId).hasMatch())
                {
                    return false;
                }

                if (existingId.left(6) == datePrefix)
                {
                    int number = existingId.right(3).toInt();

                    if (number > highestNumber)
                        highestNumber = number;
                }
            }
        }
        else if (file.exists())
        {
            return false;
        }

        if (highestNumber >= 999)
            return false;

        highestNumber++;

        QString sequence =
            QString("%1").arg(highestNumber, 3, 10, QChar('0'));

        id = datePrefix + sequence;

        return true;
    }
}
