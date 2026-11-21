// Owner: Umar Khan - tests for the parts of the order flow that had no test:
// order and bill IDs, saving and reading orders, changing an order's status
// (on-hold -> paid), the payment methods and the date/time format.
// Every file is written in a temporary folder; Data/ is never touched.

#include "../common/date_format.h"
#include "../common/id_generator.h"
#include "../order/order_repository.h"
#include "../payment/payment_method.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

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

bool writeFile(const QString &path, const QString &text)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(text.toUtf8());
    return true;
}

Pos::Order makeOrder(const QString &id, const QString &status, double amount)
{
    Pos::Order order;
    order.id = id;
    order.date = QStringLiteral("2026-11-21");
    order.time = QStringLiteral("12:30:00");
    order.customerName = QStringLiteral("Sara Ahmed");
    order.customerContact = QStringLiteral("03001234567");
    order.status = status;
    order.paymentAmount = amount;
    order.paymentMode = QStringLiteral("card");
    order.merchant = QStringLiteral("Meezan Bank");
    return order;
}

Pos::OrderDetail makeDetail(const QString &orderId, const QString &productId,
    const QString &barcode, int quantity)
{
    Pos::OrderDetail detail;
    detail.orderId = orderId;
    detail.productId = productId;
    detail.barcode = barcode;
    detail.quantity = quantity;
    return detail;
}

void testOrderIds(const QTemporaryDir &dir)
{
    const QDate day(2026, 11, 21);
    const QString file = dir.filePath(QStringLiteral("OrderIds.txt"));
    QString id;
    QString error;

    check(Pos::generateOrderId(file, day, id, error) && id == QStringLiteral("00000121"),
        QStringLiteral("The first order ID must be 00000121 on the 21st, got '%1'.").arg(id));

    writeFile(file,
        QStringLiteral("00000116 2026-11-16 10:15:00 A - paid 1.00 cash -#\n"
                       "00004219 2026-11-19 11:00:00 B - paid 1.00 cash -#\n"));
    check(Pos::generateOrderId(file, day, id, error) && id == QStringLiteral("00004321"),
        QStringLiteral("The order ID must continue after the highest one (00004321), got '%1'.").arg(id));

    check(!Pos::generateOrderId(file, QDate(), id, error),
        QStringLiteral("An order ID was made without a valid date."));

    writeFile(file, QStringLiteral("not an order#\n"));
    check(!Pos::generateOrderId(file, day, id, error),
        QStringLiteral("An order ID was made from a broken Order.txt."));
}

void testBillIds(const QTemporaryDir &dir)
{
    const QDate day(2026, 11, 21);
    const QString file = dir.filePath(QStringLiteral("BillIds.txt"));
    QString id;
    QString error;

    check(Pos::generateBillId(file, day, id, error) && id == QStringLiteral("261121001"),
        QStringLiteral("The first bill ID of the day must be 261121001, got '%1'.").arg(id));

    writeFile(file,
        QStringLiteral("261120009 A 00000120 APL 1 0 1 1 0 0 1 paid#\n"
                       "261121004 B 00000221 APL 1 0 1 1 0 0 1 paid#\n"));
    check(Pos::generateBillId(file, day, id, error) && id == QStringLiteral("261121005"),
        QStringLiteral("The bill ID must continue today's numbers (261121005), got '%1'.").arg(id));
    check(Pos::generateBillId(file, QDate(2026, 11, 22), id, error) && id == QStringLiteral("261122001"),
        QStringLiteral("A new day must start again at 001, got '%1'.").arg(id));
}

void testSaveAndRead(const QTemporaryDir &dir)
{
    const QString orders = dir.filePath(QStringLiteral("Order.txt"));
    const QString details = dir.filePath(QStringLiteral("OrderDetail.txt"));
    QString error;

    // A missing file is simply "no orders yet".
    QVector<Pos::Order> read;
    check(Pos::readOrders(orders, read, error) && read.isEmpty(),
        QStringLiteral("A missing Order.txt must read as no orders."));

    // An existing file whose last line has no line break must not get the
    // new record glued onto that line.
    writeFile(orders, QStringLiteral("00000116 2026-11-16 10:15:00 Ayesha_Khan 03001234567 paid 505.00 cash -#"));
    writeFile(details, QStringLiteral("00000116 P0001 100001 2#"));

    const Pos::Order held = makeOrder(QStringLiteral("00000221"), QStringLiteral("on-hold"), 0);
    const QVector<Pos::OrderDetail> heldItems = {
        makeDetail(held.id, QStringLiteral("P0001"), QStringLiteral("100001"), 2),
        makeDetail(held.id, QStringLiteral("P0003"), QStringLiteral("100003"), 1),
    };
    check(Pos::saveOrder(orders, details, held, heldItems, error),
        QStringLiteral("A valid held order was not saved: %1").arg(error));

    check(Pos::readOrders(orders, read, error) && read.size() == 2,
        QStringLiteral("Order.txt must hold 2 orders after saving one, got %1 (%2).").arg(read.size()).arg(error));
    if (read.size() == 2)
    {
        const Pos::Order &saved = read.at(1);
        check(saved.id == held.id && saved.status == QStringLiteral("on-hold"),
            QStringLiteral("The saved order came back with the wrong id or status."));
        check(saved.customerName == QStringLiteral("Sara_Ahmed"),
            QStringLiteral("Spaces in the customer name must be stored as underscores."));
        check(saved.customerContact == QStringLiteral("03001234567") &&
                saved.paymentMode == QStringLiteral("card") &&
                saved.merchant == QStringLiteral("Meezan_Bank"),
            QStringLiteral("The saved order lost its contact, payment mode or bank."));
    }

    QVector<Pos::OrderDetail> items;
    check(Pos::readOrderDetails(details, held.id, items, error) && items.size() == 2 &&
            items.at(0).quantity == 2 && items.at(1).productId == QStringLiteral("P0003"),
        QStringLiteral("readOrderDetails did not return the 2 items of the held order."));
    check(Pos::readOrderDetails(details, QStringLiteral("00000116"), items, error) && items.size() == 1,
        QStringLiteral("readOrderDetails mixed up the items of different orders."));
    check(Pos::readOrderDetails(details, QStringLiteral("99999999"), items, error) && items.isEmpty(),
        QStringLiteral("An unknown order must have no items."));

    // Orders that must be refused.
    check(!Pos::saveOrder(orders, details, held, heldItems, error),
        QStringLiteral("saveOrder accepted an order ID that already exists."));
    Pos::Order shortId = makeOrder(QStringLiteral("1234567"), QStringLiteral("paid"), 10);
    check(!Pos::saveOrder(orders, details, shortId,
              {makeDetail(shortId.id, QStringLiteral("P0001"), QStringLiteral("100001"), 1)}, error),
        QStringLiteral("saveOrder accepted a 7-digit order ID."));
    const Pos::Order fresh = makeOrder(QStringLiteral("00000321"), QStringLiteral("paid"), 10);
    check(!Pos::saveOrder(orders, details, fresh, {}, error),
        QStringLiteral("saveOrder accepted an order with no items."));
    check(!Pos::saveOrder(orders, details, fresh,
              {makeDetail(QStringLiteral("00000999"), QStringLiteral("P0001"), QStringLiteral("100001"), 1)}, error),
        QStringLiteral("saveOrder accepted an item that belongs to another order."));
    check(!Pos::saveOrder(orders, details, fresh,
              {makeDetail(fresh.id, QStringLiteral("P0001"), QStringLiteral("100001"), 0)}, error),
        QStringLiteral("saveOrder accepted a quantity of 0."));
    check(!Pos::saveOrder(orders, details, makeOrder(fresh.id, QStringLiteral("lost"), 10),
              {makeDetail(fresh.id, QStringLiteral("P0001"), QStringLiteral("100001"), 1)}, error),
        QStringLiteral("saveOrder accepted an unknown status."));
    check(!Pos::saveOrder(orders, details, makeOrder(fresh.id, QStringLiteral("paid"), -5),
              {makeDetail(fresh.id, QStringLiteral("P0001"), QStringLiteral("100001"), 1)}, error),
        QStringLiteral("saveOrder accepted a negative payment."));
    check(Pos::readOrders(orders, read, error) && read.size() == 2,
        QStringLiteral("A refused order still changed Order.txt."));

    // Held order is paid later: on-hold -> paid.
    check(Pos::changeOrderStatus(orders, held.id, QStringLiteral("paid"), error),
        QStringLiteral("changeOrderStatus failed: %1").arg(error));
    check(Pos::readOrders(orders, read, error) && read.size() == 2 &&
            read.at(1).status == QStringLiteral("paid") && read.at(0).status == QStringLiteral("paid") &&
            read.at(0).id == QStringLiteral("00000116"),
        QStringLiteral("changeOrderStatus did not change only the chosen order."));
    check(!Pos::changeOrderStatus(orders, QStringLiteral("99999999"), QStringLiteral("paid"), error),
        QStringLiteral("changeOrderStatus accepted an unknown order."));
    check(!Pos::changeOrderStatus(orders, held.id, QStringLiteral("refunded"), error),
        QStringLiteral("changeOrderStatus accepted an unknown status."));

    writeFile(orders, QStringLiteral("00000116 2026-11-16 paid#\n"));
    check(!Pos::readOrders(orders, read, error),
        QStringLiteral("readOrders accepted a record with missing fields."));
}

void testPayments()
{
    QString error;
    check(Pos::paymentModes() == QStringList({QStringLiteral("Cash"), QStringLiteral("Card"), QStringLiteral("Wallet")}),
        QStringLiteral("New Order must offer Cash, Card and Wallet."));
    check(!Pos::createPaymentMethod(QStringLiteral("bitcoin")),
        QStringLiteral("An unknown payment mode was accepted."));

    const auto cash = Pos::createPaymentMethod(QStringLiteral("CASH"));
    check(cash && cash->mode() == QStringLiteral("cash") && !cash->requiresMerchant(),
        QStringLiteral("Cash must be found in any letter case and need no merchant."));
    if (cash)
    {
        check(cash->validate(1000, 950, QString(), error),
            QStringLiteral("Cash above the total must be accepted (change is given)."));
        check(cash->validate(950, 950, QString(), error),
            QStringLiteral("Cash equal to the total must be accepted."));
        check(!cash->validate(900, 950, QString(), error),
            QStringLiteral("Cash below the total must be refused."));
        check(!cash->validate(10, 0, QString(), error),
            QStringLiteral("A bill total of 0 must be refused."));
    }

    for (const QString &mode : {QStringLiteral("card"), QStringLiteral("wallet")})
    {
        const auto method = Pos::createPaymentMethod(mode);
        check(method && method->mode() == mode && method->requiresMerchant(),
            QStringLiteral("%1 must need a merchant name.").arg(mode));
        if (!method)
            continue;
        check(method->validate(950, 950, QStringLiteral("HBL"), error),
            QStringLiteral("%1: the exact amount with a merchant must be accepted.").arg(mode));
        check(!method->validate(950, 950, QStringLiteral("  "), error),
            QStringLiteral("%1: an empty merchant must be refused.").arg(mode));
        check(!method->validate(950, 950, QStringLiteral("HB#L"), error),
            QStringLiteral("%1: a merchant with # would break the file and must be refused.").arg(mode));
        check(!method->validate(1000, 950, QStringLiteral("HBL"), error),
            QStringLiteral("%1: a different amount must be refused.").arg(mode));
    }
}

void testDateFormat()
{
    const QDateTime when(QDate(2026, 11, 9), QTime(14, 5, 9));
    check(Pos::formatOrderDate(when) == QStringLiteral("2026-11-09"),
        QStringLiteral("Order date must look like 2026-11-09, got '%1'.").arg(Pos::formatOrderDate(when)));
    check(Pos::formatOrderTime(when) == QStringLiteral("14:05:09"),
        QStringLiteral("Order time must be 24-hour like 14:05:09, got '%1'.").arg(Pos::formatOrderTime(when)));
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    if (!dir.isValid())
    {
        QTextStream(stderr) << "Could not create a temporary folder." << Qt::endl;
        return 1;
    }

    testOrderIds(dir);
    testBillIds(dir);
    testSaveAndRead(dir);
    testPayments();
    testDateFormat();

    if (failures > 0)
    {
        QTextStream(stderr) << failures << " check(s) failed." << Qt::endl;
        return 1;
    }
    QTextStream(stdout) << "Order repository test passed." << Qt::endl;
    return 0;
}
