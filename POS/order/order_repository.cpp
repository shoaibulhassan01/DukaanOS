
#include "order_repository.h"
#include "../common/record_codec.h"

#include <QFile>
#include <QSaveFile>
#include <QTextStream>
#include <QRegularExpression>

#include <cmath>

namespace Pos
{
    // Adds a line break at the end of the file if its last line has none.
    static bool endFileWithNewline(const QString &path, QString &error)
    {
        QFile file(path);
        if (!file.exists() || file.size() == 0)
            return true;
        if (!file.open(QIODevice::ReadWrite))
        {
            error = QStringLiteral("Cannot open %1: %2").arg(path, file.errorString());
            return false;
        }
        file.seek(file.size() - 1);
        char last = 0;
        if (file.read(&last, 1) == 1 && last != '\n')
        {
            file.seek(file.size());
            file.write("\n");
        }
        return true;
    }

    // Check whether the order status is valid.
    static bool isValidStatus(QString status)
    {
        return status == "in-progress"
            || status == "on-hold"
            || status == "paid"
            || status == "archived";
    }

    static QStringList serializeOrder(const Order& order)
    {
        QString customerName = order.customerName;
        customerName.replace(' ', '_');
        QString merchant = order.merchant.isEmpty() ? "-" : order.merchant;
        merchant.replace(' ', '_');
        return {
            order.id,
            order.date,
            order.time,
            customerName,
            order.customerContact.isEmpty() ? "-" : order.customerContact,
            order.status,
            QString::number(order.paymentAmount, 'f', 2),
            order.paymentMode,
            merchant
        };
    }

    static bool writeRecords(const QString& path, const QStringList& records,
        QString& error)
    {
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            error = QStringLiteral("Cannot update %1: %2")
                .arg(path, file.errorString());
            return false;
        }
        QTextStream output(&file);
        for (const QString& record : records)
            output << record << '\n';
        output.flush();
        if (output.status() != QTextStream::Ok || !file.commit())
        {
            error = QStringLiteral("Failed to save %1: %2")
                .arg(path, file.errorString());
            return false;
        }
        return true;
    }

    static bool readAllOrderDetails(const QString& path,
        QVector<OrderDetail>& details, QString& error)
    {
        QFile file(path);
        if (!file.exists())
        {
            details.clear();
            return true;
        }
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            error = QStringLiteral("Cannot open %1: %2")
                .arg(path, file.errorString());
            return false;
        }
        QVector<OrderDetail> loaded;
        QTextStream input(&file);
        int lineNumber = 0;
        while (!input.atEnd())
        {
            const QString line = input.readLine().trimmed();
            ++lineNumber;
            if (line.isEmpty())
                continue;
            QStringList fields;
            bool validQuantity = false;
            if (!splitRecord(line, fields) || fields.size() != 4)
            {
                error = QStringLiteral("Invalid OrderDetail.txt record at line %1.")
                    .arg(lineNumber);
                return false;
            }
            OrderDetail detail;
            detail.orderId = fields.at(0);
            detail.productId = fields.at(1);
            detail.barcode = fields.at(2);
            detail.quantity = fields.at(3).toInt(&validQuantity);
            if (detail.orderId.isEmpty() || detail.productId.isEmpty()
                || detail.barcode.isEmpty() || !validQuantity
                || detail.quantity <= 0)
            {
                error = QStringLiteral("Invalid order detail at line %1.")
                    .arg(lineNumber);
                return false;
            }
            loaded.append(detail);
        }
        details = loaded;
        return true;
    }

    // Owner: Zain Ul Abideen - read Order.txt into the list.
    bool readOrders(QString filePath, QVector<Order>& orders, QString& error)
    {
        QFile file(filePath);

        if (!file.exists())
        {
            orders.clear();
            error.clear();
            return true;
        }

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            error = QStringLiteral("Cannot open %1: %2")
                .arg(filePath, file.errorString());
            return false;
        }

        QVector<Order> loadedOrders;
        QTextStream input(&file);
        int lineNumber = 0;

        while (!input.atEnd())
        {
            QString line = input.readLine().trimmed();
            lineNumber++;

            if (line.isEmpty())
                continue;

            QStringList fields;

            if (!splitRecord(line, fields) || fields.size() != 9)
            {
                error = QStringLiteral("Invalid Order.txt record at line %1.")
                    .arg(lineNumber);
                return false;
            }

            bool validAmount = false;

            Order order;

            order.id = fields.at(0);
            order.date = fields.at(1);
            order.time = fields.at(2);
            order.customerName = fields.at(3);
            order.customerContact = fields.at(4) == "-"
                ? "" : fields.at(4);
            order.status = fields.at(5);
            order.paymentAmount = fields.at(6).toDouble(&validAmount);
            order.paymentMode = fields.at(7);
            order.merchant = fields.at(8) == "-"
                ? "" : fields.at(8);

            if (!QRegularExpression("^\\d{8}$").match(order.id).hasMatch()
                || !validAmount
                || !std::isfinite(order.paymentAmount)
                || order.paymentAmount < 0
                || !isValidStatus(order.status))
            {
                error = QStringLiteral("Invalid order information at line %1.")
                    .arg(lineNumber);
                return false;
            }

            loadedOrders.append(order);
        }

        orders = loadedOrders;
        error.clear();

        return true;
    }

    // Owner: Zain Ul Abideen - read detail rows for one order ID.
    bool readOrderDetails(QString filePath, QString orderId,
        QVector<OrderDetail>& details, QString& error)
    {
        QFile file(filePath);

        if (!file.exists())
        {
            details.clear();
            error.clear();
            return true;
        }

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            error = QStringLiteral("Cannot open %1: %2")
                .arg(filePath, file.errorString());
            return false;
        }

        QVector<OrderDetail> loadedDetails;
        QTextStream input(&file);
        int lineNumber = 0;

        while (!input.atEnd())
        {
            QString line = input.readLine().trimmed();
            lineNumber++;

            if (line.isEmpty())
                continue;

            QStringList fields;

            if (!splitRecord(line, fields) || fields.size() != 4)
            {
                error = QStringLiteral("Invalid OrderDetail.txt record at line %1.")
                    .arg(lineNumber);
                return false;
            }

            bool validQuantity = false;

            OrderDetail detail;

            detail.orderId = fields.at(0);
            detail.productId = fields.at(1);
            detail.barcode = fields.at(2);
            detail.quantity = fields.at(3).toInt(&validQuantity);

            if (!validQuantity || detail.quantity <= 0)
            {
                error = QStringLiteral("Invalid quantity at line %1.")
                    .arg(lineNumber);
                return false;
            }

            if (detail.orderId == orderId)
            {
                loadedDetails.append(detail);
            }
        }

        details = loadedDetails;
        error.clear();

        return true;
    }

    // Owner: Zain Ul Abideen - save one order and its detail rows.
    bool saveOrder(QString orderFile, QString detailFile, Order order,
        QVector<OrderDetail> details, QString& error)
    {
        if (!QRegularExpression("^\\d{8}$").match(order.id).hasMatch())
        {
            error = QStringLiteral("Order ID must contain 8 digits.");
            return false;
        }

        if (details.isEmpty())
        {
            error = QStringLiteral("Order must contain at least one product.");
            return false;
        }

        if (order.customerName.trimmed().isEmpty()
            || !std::isfinite(order.paymentAmount)
            || order.paymentAmount < 0
            || !isValidStatus(order.status))
        {
            error = QStringLiteral("Invalid order information.");
            return false;
        }

        for (const OrderDetail& detail : details)
        {
            if (detail.orderId != order.id
                || detail.productId.isEmpty()
                || detail.barcode.isEmpty()
                || detail.quantity <= 0)
            {
                error = QStringLiteral("Invalid order detail information.");
                return false;
            }
        }

        // Check that the order ID is not already present.
        QVector<Order> existingOrders;

        if (!readOrders(orderFile, existingOrders, error))
            return false;

        for (const Order& existing : existingOrders)
        {
            if (existing.id == order.id)
            {
                error = QStringLiteral("This order ID already exists.");
                return false;
            }
        }

        // Convert spaces into underscores for the file format.
        QString customerName = order.customerName;
        customerName.replace(' ', '_');

        QString customerContact = order.customerContact.isEmpty()
            ? "-" : order.customerContact;

        QString merchant = order.merchant.isEmpty()
            ? "-" : order.merchant;

        merchant.replace(' ', '_');

        QStringList orderFields;

        orderFields << order.id
            << order.date
            << order.time
            << customerName
            << customerContact
            << order.status
            << QString::number(order.paymentAmount, 'f', 2)
            << order.paymentMode
            << merchant;

        // A file whose last line has no line break would get the new record
        // glued onto that line, so add the missing break first.
        for (const QString &path : { orderFile, detailFile })
        {
            if (!endFileWithNewline(path, error))
                return false;
        }

        QFile orderOutput(orderFile);
        QFile detailOutput(detailFile);

        // Open both files before writing.
        if (!orderOutput.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        {
            error = QStringLiteral("Cannot open Order.txt: %1")
                .arg(orderOutput.errorString());
            return false;
        }

        if (!detailOutput.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        {
            error = QStringLiteral("Cannot open OrderDetail.txt: %1")
                .arg(detailOutput.errorString());
            return false;
        }

        QTextStream orderStream(&orderOutput);
        QTextStream detailStream(&detailOutput);

        orderStream << makeRecord(orderFields) << '\n';

        if (orderStream.status() != QTextStream::Ok || !orderOutput.flush())
        {
            error = QStringLiteral("Failed to write Order.txt.");
            return false;
        }

        // Write one detail record for each product.
        for (const OrderDetail& detail : details)
        {
            QStringList detailFields;

            detailFields << detail.orderId
                << detail.productId
                << detail.barcode
                << QString::number(detail.quantity);

            detailStream << makeRecord(detailFields) << '\n';
        }

        if (detailStream.status() != QTextStream::Ok || !detailOutput.flush())
        {
            error = QStringLiteral("Failed to write OrderDetail.txt.");
            return false;
        }

        error.clear();

        return true;
    }

    bool updateOrder(QString orderFile, QString detailFile, Order order,
        QVector<OrderDetail> details, QString& error)
    {
        if (!QRegularExpression("^\\d{8}$").match(order.id).hasMatch()
            || details.isEmpty()
            || order.customerName.trimmed().isEmpty()
            || !std::isfinite(order.paymentAmount)
            || order.paymentAmount < 0
            || !isValidStatus(order.status))
        {
            error = QStringLiteral("Invalid order information.");
            return false;
        }
        for (const OrderDetail& detail : details)
        {
            if (detail.orderId != order.id || detail.productId.isEmpty()
                || detail.barcode.isEmpty() || detail.quantity <= 0)
            {
                error = QStringLiteral("Invalid order detail information.");
                return false;
            }
        }

        QVector<Order> orders;
        if (!readOrders(orderFile, orders, error))
            return false;
        int matchingOrder = -1;
        for (int i = 0; i < orders.size(); ++i)
        {
            if (orders.at(i).id != order.id)
                continue;
            if (matchingOrder >= 0)
            {
                error = QStringLiteral("Order ID %1 appears more than once.")
                    .arg(order.id);
                return false;
            }
            matchingOrder = i;
        }
        if (matchingOrder < 0)
        {
            error = QStringLiteral("Order ID was not found.");
            return false;
        }
        orders[matchingOrder] = order;

        QVector<OrderDetail> allDetails;
        if (!readAllOrderDetails(detailFile, allDetails, error))
            return false;
        QStringList originalDetailRecords;
        for (const OrderDetail& detail : allDetails)
        {
            originalDetailRecords.append(makeRecord({
                detail.orderId, detail.productId, detail.barcode,
                QString::number(detail.quantity)
            }));
        }
        QVector<OrderDetail> replacedDetails;
        for (const OrderDetail& detail : allDetails)
        {
            if (detail.orderId != order.id)
                replacedDetails.append(detail);
        }
        for (const OrderDetail& detail : details)
            replacedDetails.append(detail);

        QStringList detailRecords;
        for (const OrderDetail& detail : replacedDetails)
        {
            detailRecords.append(makeRecord({
                detail.orderId, detail.productId, detail.barcode,
                QString::number(detail.quantity)
            }));
        }
        if (!writeRecords(detailFile, detailRecords, error))
            return false;

        QStringList orderRecords;
        for (const Order& existing : orders)
            orderRecords.append(makeRecord(serializeOrder(existing)));
        if (!writeRecords(orderFile, orderRecords, error))
        {
            const QString saveError = error;
            QString rollbackError;
            if (!writeRecords(detailFile, originalDetailRecords, rollbackError))
                error = QStringLiteral("%1 Order details could not be restored: %2")
                    .arg(saveError, rollbackError);
            else
                error = saveError;
            return false;
        }

        error.clear();
        return true;
    }

    // Owner: Zain Ul Abideen - change an order's status in Order.txt.
    bool changeOrderStatus(QString filePath, QString orderId,
        QString status, QString& error)
    {
        if (!isValidStatus(status))
        {
            error = QStringLiteral("Invalid order status.");
            return false;
        }

        QVector<Order> orders;

        if (!readOrders(filePath, orders, error))
            return false;

        bool found = false;

        for (Order& order : orders)
        {
            if (order.id == orderId)
            {
                order.status = status;
                found = true;
                break;
            }
        }

        if (!found)
        {
            error = QStringLiteral("Order ID was not found.");
            return false;
        }

        QSaveFile file(filePath);

        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            error = QStringLiteral("Cannot update Order.txt: %1")
                .arg(file.errorString());
            return false;
        }

        QTextStream output(&file);

        for (const Order& order : orders)
        {
            output << makeRecord(serializeOrder(order)) << '\n';
        }

        output.flush();

        if (output.status() != QTextStream::Ok)
        {
            error = QStringLiteral("Failed to write updated Order.txt.");
            return false;
        }

        if (!file.commit())
        {
            error = QStringLiteral("Failed to save updated Order.txt: %1")
                .arg(file.errorString());
            return false;
        }

        error.clear();

        return true;
    }
}
