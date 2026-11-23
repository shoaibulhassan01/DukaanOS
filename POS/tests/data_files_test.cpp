// Owner: Umar Khan - checks the sample data in the project's Data folder, so
// a broken or inconsistent record is caught in the pull request instead of
// on demo day. It only reads the files.
//
// Checks: every file reads; users have a known role and there is at least one
// manager and one cashier; every product passes validateProduct, ids and
// barcodes are unique and the barcode picture exists; order ids are unique;
// every order item points to a real order and product (same barcode); every
// each bill header and detail belongs together and totals add up; a paid
// order's amount equals the total of its bill.

#include "../bill/bill.h"
#include "../common/record_codec.h"
#include "../order/order_repository.h"
#include "../product/product_repository.h"
#include "../product/product_service.h"
#include "../user/auth_service.h"
#include "../user/user_repository.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>

#include <cmath>

#ifndef DUKAANOS_SOURCE_DIR
#error "DUKAANOS_SOURCE_DIR must point at the project folder (set in CMakeLists.txt)."
#endif

namespace
{
int failures = 0;

void check(bool condition, const QString &message)
{
    if (!condition)
    {
        QTextStream(stderr) << "FAIL: " << message << Qt::endl;
        ++failures;
    }
}

bool sameMoney(double a, double b)
{
    return std::abs(a - b) < 0.01;
}

// All records of a file, already split into fields.
QVector<QStringList> readRecords(const QString &path, int fieldCount)
{
    QVector<QStringList> records;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        check(false, QStringLiteral("Cannot open %1.").arg(path));
        return records;
    }
    QTextStream input(&file);
    int lineNumber = 0;
    while (!input.atEnd())
    {
        const QString line = input.readLine().trimmed();
        ++lineNumber;
        if (line.isEmpty())
            continue;
        QStringList fields;
        if (!Pos::splitRecord(line, fields) || fields.size() != fieldCount)
        {
            check(false, QStringLiteral("%1 line %2 does not have %3 fields.")
                .arg(QFileInfo(path).fileName()).arg(lineNumber).arg(fieldCount));
            continue;
        }
        records.append(fields);
    }
    return records;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QDir project(QStringLiteral(DUKAANOS_SOURCE_DIR));
    const QDir data(project.filePath(QStringLiteral("Data")));
    QString error;

    // ------------------------------------------------------------- Users.txt
    QVector<Pos::User> users;
    check(Pos::readUsers(data.filePath(QStringLiteral("Users.txt")), users, error),
        QStringLiteral("Users.txt does not read: %1").arg(error));
    Pos::User sampleManager;
    check(Pos::login(data.filePath(QStringLiteral("Users.txt")), QStringLiteral("manager"),
            QStringLiteral("manager123"), sampleManager, error),
        QStringLiteral("Sample manager password hash does not verify: %1").arg(error));
    QSet<QString> usernames;
    int managers = 0;
    int cashiers = 0;
    for (const Pos::User &user : users)
    {
        check(!usernames.contains(user.username),
            QStringLiteral("Username '%1' appears twice in Users.txt.").arg(user.username));
        usernames.insert(user.username);
        check(!user.password.isEmpty(),
            QStringLiteral("User '%1' has no password.").arg(user.username));
        if (user.role == QStringLiteral("manager"))
            ++managers;
        else if (user.role == QStringLiteral("cashier"))
            ++cashiers;
        else
            check(false, QStringLiteral("User '%1' has unknown role '%2'.").arg(user.username, user.role));
    }
    check(managers > 0 && cashiers > 0,
        QStringLiteral("Users.txt needs at least one manager and one cashier for the demo."));

    // ----------------------------------------------------------- Product.txt
    QVector<Pos::Product> products;
    check(Pos::readProducts(data.filePath(QStringLiteral("Product.txt")), products, error),
        QStringLiteral("Product.txt does not read: %1").arg(error));
    check(!products.isEmpty(), QStringLiteral("Product.txt has no products."));
    QHash<QString, Pos::Product> productById;
    QSet<QString> barcodes;
    for (const Pos::Product &product : products)
    {
        QString why;
        const bool valid = Pos::validateProduct(product, why);   // fills why first
        check(valid, QStringLiteral("Product %1 is not valid: %2").arg(product.id, why));
        check(!productById.contains(product.id),
            QStringLiteral("Product id %1 appears twice.").arg(product.id));
        check(!barcodes.contains(product.barcode),
            QStringLiteral("Barcode %1 is used by two products.").arg(product.barcode));
        productById.insert(product.id, product);
        barcodes.insert(product.barcode);
        check(QFileInfo::exists(project.filePath(product.barcodeImage)),
            QStringLiteral("Product %1: barcode picture %2 is missing.").arg(product.id, product.barcodeImage));
    }

    // ------------------------------------------------------------- Order.txt
    QVector<Pos::Order> orders;
    check(Pos::readOrders(data.filePath(QStringLiteral("Order.txt")), orders, error),
        QStringLiteral("Order.txt does not read: %1").arg(error));
    QHash<QString, Pos::Order> orderById;
    for (const Pos::Order &order : orders)
    {
        check(!orderById.contains(order.id),
            QStringLiteral("Order id %1 appears twice.").arg(order.id));
        orderById.insert(order.id, order);
    }

    // ------------------------------------------------------- OrderDetail.txt
    QSet<QString> ordersWithItems;
    for (const QStringList &item : readRecords(data.filePath(QStringLiteral("OrderDetail.txt")), 4))
    {
        const QString &orderId = item.at(0);
        const QString &productId = item.at(1);
        ordersWithItems.insert(orderId);
        check(orderById.contains(orderId),
            QStringLiteral("OrderDetail.txt has an item for order %1, which is not in Order.txt.").arg(orderId));
        check(productById.contains(productId),
            QStringLiteral("Order %1 has product %2, which is not in Product.txt.").arg(orderId, productId));
        if (productById.contains(productId))
            check(productById.value(productId).barcode == item.at(2),
                QStringLiteral("Order %1: barcode %2 does not match product %3.").arg(orderId, item.at(2), productId));
        check(item.at(3).toInt() > 0,
            QStringLiteral("Order %1 has a quantity that is not above 0.").arg(orderId));
    }
    for (const Pos::Order &order : orders)
        check(ordersWithItems.contains(order.id),
            QStringLiteral("Order %1 has no items in OrderDetail.txt.").arg(order.id));

        // -------------------------------------------------------------- Bill.txt
        QHash<QString, double> billTotalByOrder;
        QHash<QString, QStringList> billById;
        QSet<QString> billIds;
        for (const QStringList &row : readRecords(data.filePath(QStringLiteral("Bill.txt")), 14))
    {
        const QString &billId = row.at(0);
        const QString &orderId = row.at(2);
            check(QRegularExpression(QStringLiteral("^\\d{9}$")).match(billId).hasMatch(),
                QStringLiteral("Bill ID %1 must contain 9 digits.").arg(billId));
            check(!billIds.contains(billId),
                QStringLiteral("Bill ID %1 appears more than once.").arg(billId));
            billIds.insert(billId);
        check(orderById.contains(orderId),
            QStringLiteral("Bill %1 belongs to order %2, which is not in Order.txt.").arg(billId, orderId));
            const double withoutTax = row.at(3).toDouble();
            const double tax = row.at(4).toDouble();
            const double discount = row.at(5).toDouble();
            const double payable = row.at(6).toDouble();
        check(sameMoney(withoutTax + tax - discount, payable),
                QStringLiteral("Bill %1: %2 + %3 - %4 is not %5.")
                    .arg(billId, row.at(3), row.at(4), row.at(5), row.at(6)));
            if (row.at(9) == QStringLiteral("paid"))
            {
                check(sameMoney(row.at(7).toDouble() - row.at(8).toDouble(), payable),
                    QStringLiteral("Bill %1 amount received minus balance returned is not payable.")
                        .arg(billId));
            }
        billTotalByOrder[orderId] += payable;
            billById.insert(billId, row);
        }
        QHash<QString, double> detailSubtotalByBill;
        QHash<QString, double> detailDiscountByBill;
        QSet<QString> billsWithDetails;
        for (const QStringList &row : readRecords(
                 data.filePath(QStringLiteral("BillDetail.txt")), 5))
        {
            const QString &billId = row.at(0);
            const double unitPrice = row.at(2).toDouble();
            const int quantity = row.at(3).toInt();
            const double discount = row.at(4).toDouble();
            check(billById.contains(billId),
                QStringLiteral("BillDetail.txt references unknown bill %1.").arg(billId));
            check(quantity > 0 && unitPrice >= 0 && discount >= 0
                    && discount <= unitPrice * quantity,
                QStringLiteral("Bill %1 has invalid product detail amounts.").arg(billId));
            detailSubtotalByBill[billId] += unitPrice * quantity;
            detailDiscountByBill[billId] += discount;
            billsWithDetails.insert(billId);
        }
        for (auto it = billById.cbegin(); it != billById.cend(); ++it)
        {
            check(billsWithDetails.contains(it.key()),
                QStringLiteral("Bill %1 has no BillDetail rows.").arg(it.key()));
            check(sameMoney(detailSubtotalByBill.value(it.key()), it.value().at(3).toDouble()),
                QStringLiteral("Bill %1 subtotal does not match BillDetail.txt.").arg(it.key()));
            check(sameMoney(detailDiscountByBill.value(it.key()), it.value().at(5).toDouble()),
                QStringLiteral("Bill %1 discount does not match BillDetail.txt.").arg(it.key()));
    }
    for (const Pos::Order &order : orders)
    {
        if (order.status != QStringLiteral("paid"))
            continue;
        check(billTotalByOrder.contains(order.id),
            QStringLiteral("Paid order %1 has no bill.").arg(order.id));
        check(!billTotalByOrder.contains(order.id) || sameMoney(billTotalByOrder.value(order.id), order.paymentAmount),
            QStringLiteral("Paid order %1: amount %2 does not match its bill total %3.")
                .arg(order.id).arg(order.paymentAmount, 0, 'f', 2)
                .arg(billTotalByOrder.value(order.id), 0, 'f', 2));
    }

    if (failures > 0)
    {
        QTextStream(stderr) << failures << " problem(s) found in Data/." << Qt::endl;
        return 1;
    }
    QTextStream(stdout) << "Data files test passed: " << users.size() << " users, "
                        << products.size() << " products, " << orders.size() << " orders." << Qt::endl;
    return 0;
}
