#include "order_repository.h"

#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

namespace Pos
{
// Read orders from the selected text file.
bool readOrders(QString filePath, QVector<Order> &orders, QString &error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        error = QStringLiteral("Cannot open %1: %2").arg(filePath, file.errorString());
        return false;
    }

    QVector<Order> loadedOrders;
    QTextStream input(&file);
    int lineNumber = 0;
    const QRegularExpression orderIdPattern(QStringLiteral("^\\d{8}$"));
    while (!input.atEnd())
    {
        const QString line = input.readLine().trimmed();
        ++lineNumber;
        if (line.isEmpty())
            continue;
        if (!line.endsWith(QLatin1Char('#')))
        {
            error = QStringLiteral("Order.txt line %1 is missing the ending #.")
                .arg(lineNumber);
            return false;
        }

        const QStringList fields = line.left(line.size() - 1)
            .split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (fields.size() != 9)
        {
            error = QStringLiteral("Order.txt line %1 must contain 9 fields.")
                .arg(lineNumber);
            return false;
        }

        bool validPaymentAmount = false;
        Order order;
        order.id = fields.at(0);
        order.date = fields.at(1);
        order.time = fields.at(2);
        order.customerName = fields.at(3);
        order.customerContact = fields.at(4) == QStringLiteral("-")
            ? QString() : fields.at(4);
        order.status = fields.at(5);
        order.paymentAmount = fields.at(6).toDouble(&validPaymentAmount);
        order.paymentMode = fields.at(7);
        order.merchant = fields.at(8) == QStringLiteral("-")
            ? QString() : fields.at(8);
        if (!orderIdPattern.match(order.id).hasMatch() || !validPaymentAmount)
        {
            error = QStringLiteral("Order.txt line %1 has an invalid order ID or payment amount.")
                .arg(lineNumber);
            return false;
        }

        loadedOrders.append(order);
    }

    orders = loadedOrders;
    error.clear();
    return true;
}

// Read detail rows for one order ID.
bool readOrderDetails(QString filePath, QString orderId,
    QVector<OrderDetail> &details, QString &error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        error = QStringLiteral("Cannot open %1: %2").arg(filePath, file.errorString());
        return false;
    }

    QVector<OrderDetail> loadedDetails;
    QTextStream input(&file);
    int lineNumber = 0;
    while (!input.atEnd())
    {
        const QString line = input.readLine().trimmed();
        ++lineNumber;
        if (line.isEmpty())
            continue;
        if (!line.endsWith(QLatin1Char('#')))
        {
            error = QStringLiteral("OrderDetail.txt line %1 is missing the ending #.")
                .arg(lineNumber);
            return false;
        }

        const QStringList fields = line.left(line.size() - 1)
            .split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (fields.size() != 4)
        {
            error = QStringLiteral("OrderDetail.txt line %1 must contain 4 fields.")
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
            error = QStringLiteral("OrderDetail.txt line %1 has an invalid quantity.")
                .arg(lineNumber);
            return false;
        }
        if (detail.orderId == orderId)
            loadedDetails.append(detail);
    }

    details = loadedDetails;
    error.clear();
    return true;
}

// TODO: Save one order and its detail rows.
bool saveOrder(QString orderFile, QString detailFile, Order order,
    QVector<OrderDetail> details, QString &error)
{
    error = QStringLiteral("TODO: Save order and order details.");
    return false;
}

// TODO: Change an order's status in Order.txt.
bool changeOrderStatus(QString filePath, QString orderId, QString status,
    QString &error)
{
    error = QStringLiteral("TODO: Update the order status.");
    return false;
}
}
