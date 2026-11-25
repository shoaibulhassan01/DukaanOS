// Bill persistence, checkout and held-order tests. All writes use temp folders.
#include "../bill/bill_repository.h"
#include "../bill/bill_service.h"
#include "../checkout/checkout_service.h"
#include "../common/data_paths.h"
#include "../common/id_generator.h"
#include "../order/order_repository.h"
#include "../order/order_service.h"
#include "../product/product_repository.h"

#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTextStream>
#include <cstdio>

using namespace Pos;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
    std::printf("FAIL line %d: %s\n", __LINE__, #cond); ++failures; } } while (0)

static Product makeProduct(const QString &id, const QString &barcode,
    const QString &acronym, double price, double tax, double discount, int qty)
{
    Product product;
    product.id = id;
    product.barcode = barcode;
    product.acronym = acronym;
    product.unitPrice = price;
    product.tax = tax;
    product.discountPerUnit = discount;
    product.quantity = qty;
    return product;
}

static QStringList readLines(const QString &path)
{
    QStringList lines;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return lines;
    QTextStream in(&file);
    while (!in.atEnd())
    {
        const QString line = in.readLine();
        if (!line.trimmed().isEmpty())
            lines.append(line);
    }
    return lines;
}

static bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return file.write(contents) == contents.size();
}

static void testBillCalculations()
{
    QString error;
    BillLine milk;
    CHECK(calculateBillLine(QStringLiteral("000000001"),
        makeProduct("P001", "100001", "MLK1L", 280, 14, 0, 50),
        2, milk, error));
    CHECK(milk.totalWithoutTax == 560 && milk.totalTax == 28
        && milk.detail.totalDiscount == 0 && milk.totalPayable == 588);

    BillLine sugar;
    CHECK(calculateBillLine(QStringLiteral("000000001"),
        makeProduct("P002", "100002", "SUGR1KG", 160, 8, 5, 40),
        3, sugar, error));
    CHECK(sugar.detail.totalDiscount == 15 && sugar.totalPayable == 489);
    CHECK(!calculateBillLine("1",
        makeProduct("P", "1", "X", 10, 0, 11, 1), 1, sugar, error));
    CHECK(!calculateBillLine("1",
        makeProduct("P", "1", "X", 10, 0, 0, 1), 0, sugar, error));

    Bill bill;
    Order order;
    order.id = QStringLiteral("00000119");
    order.customerName = QStringLiteral("Ali Raza");
    order.status = QStringLiteral("paid");
    CHECK(buildBill("000000001", order, {
        {makeProduct("P001", "100001", "MLK1L", 280, 14, 0, 50), 2},
        {makeProduct("P002", "100002", "SUGR1KG", 160, 8, 5, 40), 3}
    }, bill, error));
    CHECK(bill.details.size() == 2);
    CHECK(bill.totalWithoutTax == 1040 && bill.totalTax == 52);
    CHECK(bill.totalDiscount == 15 && bill.totalPayable == 1077);
    CHECK(bill.details.at(1).totalDiscount == 15);

    Bill seasonalBill;
    CHECK(buildBill("000000002", order, {
        {makeProduct("P001", "100001", "MLK1L", 280, 14, 0, 50), 2},
        {makeProduct("P002", "100002", "SUGR1KG", 160, 8, 5, 40), 3}
    }, seasonalBill, error, 10));
    CHECK(seasonalBill.seasonalDiscount == 107.70
        && seasonalBill.totalPayable == 969.30);

    Bill specialBill;
    CHECK(buildBill("000000003", order, {
        {makeProduct("P001", "100001", "MLK1L", 280, 14, 0, 50), 2},
        {makeProduct("P002", "100002", "SUGR1KG", 160, 8, 5, 40), 3}
    }, specialBill, error, 10, 8));
    CHECK(specialBill.seasonalDiscount == 107.70
        && specialBill.specialDiscount == 77.54
        && specialBill.totalPayable == 891.76);
    // Special discount: any typed amount up to 8 percent with 2 decimals
    // (req 8), e.g. 0.5 of 588.00 = 2.94.
    CHECK(buildBill("000000004", order, {
        {makeProduct("P001", "100001", "MLK1L", 280, 14, 0, 50), 2}
    }, specialBill, error, 0, 0.5));
    CHECK(specialBill.specialDiscount == 2.94 && specialBill.totalPayable == 585.06);
    CHECK(!buildBill("000000005", order, {
        {makeProduct("P001", "100001", "MLK1L", 280, 14, 0, 50), 2}
    }, specialBill, error, 0, 9));
    CHECK(!buildBill("000000006", order, {
        {makeProduct("P001", "100001", "MLK1L", 280, 14, 0, 50), 2}
    }, specialBill, error, 0, 8.01));
    CHECK(!buildBill("000000007", order, {
        {makeProduct("P001", "100001", "MLK1L", 280, 14, 0, 50), 2}
    }, specialBill, error, 0, 2.555));
}

static void testBillRepository(const QString &dir)
{
    QDir().mkpath(dir);
    const QString billFile = dir + "/Bill.txt";
    const QString detailFile = dir + "/BillDetail.txt";
    Order order;
    order.id = QStringLiteral("00000119");
    order.customerName = QStringLiteral("Ali Raza");
    order.status = QStringLiteral("paid");
    Bill bill;
    QString error;
    CHECK(buildBill("261121001", order, {
        {makeProduct("P001", "100001", "MLK1L", 280, 14, 0, 50), 2},
        {makeProduct("P002", "100002", "SUGR1KG", 160, 8, 5, 40), 3}
    }, bill, error));
    bill.amountReceived = 1080;
    bill.balanceReturned = 3;
    bill.date = QStringLiteral("2026-11-21");
    bill.time = QStringLiteral("13:00:00");

    CHECK(saveBill(billFile, detailFile, bill, error));
    const QStringList headers = readLines(billFile);
    const QStringList details = readLines(detailFile);
    CHECK(headers.size() == 1 && headers.first().endsWith('#'));
    CHECK(headers.first() == QStringLiteral(
        "261121001 Ali_Raza 00000119 1040.00 52.00 15.00 1077.00 "
        "1080.00 3.00 paid 0.00 0.00 2026-11-21 13:00:00#"));
    CHECK(headers.first().split(' ').size() == 14);
    CHECK(details == QStringList({
        QStringLiteral("261121001 MLK1L 280.00 2 0.00#"),
        QStringLiteral("261121001 SUGR1KG 160.00 3 15.00#")
    }));

    Bill readBack;
    CHECK(readBill(billFile, detailFile, bill.id, readBack, error));
    CHECK(readBack.details.size() == 2 && readBack.totalPayable == 1077);
    CHECK(readBack.details.at(1).totalDiscount == 15);
    CHECK(readBillForOrder(billFile, detailFile, order.id, readBack, error));
    CHECK(readBack.id == bill.id);

    CHECK(!saveBill(billFile, detailFile, bill, error));
    CHECK(readLines(billFile).size() == 1 && readLines(detailFile).size() == 2);
    CHECK(!readBill(billFile, detailFile, "999999999", readBack, error));

    QFile broken(billFile);
    CHECK(broken.open(QIODevice::WriteOnly | QIODevice::Truncate));
    broken.write("261121001 malformed#\n");
    broken.close();
    CHECK(!generateBillId(billFile, QDate(2026, 11, 21),
        readBack.id, error));
}

static void testCheckout(const QString &dir)
{
    QDir().mkpath(dir + "/Data");
    QDir::setCurrent(dir);
    CHECK(writeFile("Data/Product.txt",
        "P001 100001 MLK1L Milk_1L count 280.00 14.00 0.00 50 Data/barcodes/100001.png#\n"
        "P002 100002 SUGR1KG Sugar_1kg weight 160.00 8.00 5.00 40 Data/barcodes/100002.png#\n"));
    CHECK(writeFile("Data/Seasons.txt",
        QStringLiteral("2026-06-01 2026-08-31 10 false#\n").toUtf8()));

    CheckoutRequest request;
    request.customerName = QStringLiteral("Ali Raza");
    request.customerContact = QStringLiteral("03001234567");
    request.paymentMode = QStringLiteral("wallet");
    request.merchant = QStringLiteral("JazzCash");
    request.amountPaid = 1077;
    request.items = {
        {makeProduct("P001", "100001", "MLK1L", 280, 14, 0, 50), 2},
        {makeProduct("P002", "100002", "SUGR1KG", 160, 8, 5, 40), 3}
    };

    CheckoutResult result;
    QString error;
    CheckoutRequest noMerchant = request;
    noMerchant.merchant.clear();
    CHECK(!generateBill(noMerchant, result, error) && !error.isEmpty());
    CheckoutRequest excessiveDiscount = request;
    excessiveDiscount.specialDiscountPercentage = 9;
    CHECK(!generateBill(excessiveDiscount, result, error) && !error.isEmpty());
    CheckoutRequest threeDecimals = request;
    threeDecimals.specialDiscountPercentage = 2.555;   // at most 2 decimals
    CHECK(!generateBill(threeDecimals, result, error) && !error.isEmpty());
    CHECK(readLines("Data/Order.txt").isEmpty());
    CHECK(readLines("Data/Bill.txt").isEmpty());

    CHECK(generateBill(request, result, error));
    CHECK(result.totals.withoutTax == 1040 && result.totals.tax == 52);
    CHECK(result.totals.discount == 15 && result.totals.payable == 1077);
    CHECK(result.bill.id.size() == 9);
    CHECK(QRegularExpression(QStringLiteral("^\\d{9}$"))
        .match(result.bill.id).hasMatch());
    CHECK(result.bill.amountReceived == 1077
        && result.bill.balanceReturned == 0);

    CartTotals onePercentTotals;
    CHECK(calculateCartTotals(request.items, onePercentTotals, error, 1));
    CHECK(onePercentTotals.specialDiscount == 10.77
        && onePercentTotals.payable == 1066.23);
    CartTotals eightPercentTotals;
    CHECK(calculateCartTotals(request.items, eightPercentTotals, error, 8));
    CHECK(eightPercentTotals.specialDiscount == 86.16
        && eightPercentTotals.payable == 990.84);

    const QStringList orderLines = readLines("Data/Order.txt");
    const QStringList detailLines = readLines("Data/OrderDetail.txt");
    const QStringList billLines = readLines("Data/Bill.txt");
    const QStringList billDetailLines = readLines("Data/BillDetail.txt");
    CHECK(orderLines.size() == 1);
    CHECK(orderLines.first().contains("Ali_Raza 03001234567 paid 1077.00 wallet JazzCash#"));
    CHECK(detailLines.size() == 2);
    CHECK(detailLines.at(0) == result.order.id + " P001 100001 2#");
    CHECK(detailLines.at(1) == result.order.id + " P002 100002 3#");
    CHECK(billLines.size() == 1 && billLines.first().endsWith('#'));
    CHECK(billLines.first().split(' ').size() == 14);
    CHECK(billLines.first().contains(
        QStringLiteral("Ali_Raza %1 1040.00 52.00 15.00 1077.00 "
                       "1077.00 0.00 paid 0.00 0.00 ")
            .arg(result.order.id)));
    CHECK(billDetailLines == QStringList({
        result.bill.id + " MLK1L 280.00 2 0.00#",
        result.bill.id + " SUGR1KG 160.00 3 15.00#"
    }));

    Bill loaded;
    CHECK(readBill("Data/Bill.txt", "Data/BillDetail.txt",
        result.bill.id, loaded, error));
    CHECK(loaded.details.size() == 2 && loaded.totalPayable == 1077);

    QString nextBillId;
    CHECK(generateBillId("Data/Bill.txt", QDate::currentDate(),
        nextBillId, error));
    CHECK(nextBillId != result.bill.id);

    const QStringList stock = readLines("Data/Product.txt");
    CHECK(stock.at(0).contains(" 48 ") && stock.at(1).contains(" 37 "));

    CheckoutRequest tooMuch = request;
    tooMuch.items = {
        {makeProduct("P001", "100001", "MLK1L", 280, 14, 0, 50), 49}
    };
    tooMuch.amountPaid = 49 * 294.0;
    CHECK(!generateBill(tooMuch, result, error));
    CHECK(readLines("Data/Order.txt").size() == 1);
    CHECK(readLines("Data/Bill.txt").size() == 1);

    const QDate today = QDate::currentDate();
    const QString seasonStart = today.addDays(-30).toString(Qt::ISODate);
    const QString seasonEnd = today.addDays(30).toString(Qt::ISODate);
    CHECK(writeFile("Data/Seasons.txt",
        (QStringLiteral("%1 %2 10 false#\n")
            .arg(seasonStart, seasonEnd)).toUtf8()));
    CheckoutRequest seasonalRequest = request;
    seasonalRequest.amountPaid = 1077.00;
    CartTotals inactiveTotals;
    CHECK(calculateCartTotals(seasonalRequest.items, inactiveTotals, error));
    CHECK(inactiveTotals.seasonalDiscount == 0
        && inactiveTotals.payable == 1077.00);

    CHECK(writeFile("Data/Seasons.txt",
        (QStringLiteral("%1 %2 10 true#\n")
            .arg(seasonStart, seasonEnd)).toUtf8()));
    seasonalRequest.amountPaid = 969.30;
    CartTotals activeTotals;
    CHECK(calculateCartTotals(seasonalRequest.items, activeTotals, error));
    CHECK(activeTotals.seasonalDiscount == 107.70
        && activeTotals.payable == 969.30);
    seasonalRequest.specialDiscountPercentage = 8;
    seasonalRequest.amountPaid = 891.76;
    CheckoutResult seasonalResult;
    CHECK(generateBill(seasonalRequest, seasonalResult, error));
    CHECK(seasonalResult.bill.seasonalDiscount == 107.70
        && seasonalResult.bill.specialDiscount == 77.54
        && seasonalResult.bill.totalPayable == 891.76);
    CHECK(seasonalResult.totals.seasonalDiscount == 107.70
        && seasonalResult.totals.specialDiscount == 77.54
        && seasonalResult.totals.payable == 891.76);
    CHECK(seasonalResult.bill.amountReceived == 891.76
        && seasonalResult.bill.balanceReturned == 0);
    Bill savedSpecialBill;
    CHECK(readBill("Data/Bill.txt", "Data/BillDetail.txt",
        seasonalResult.bill.id, savedSpecialBill, error));
    CHECK(savedSpecialBill.specialDiscount == 77.54
        && savedSpecialBill.totalPayable == 891.76);
}

static void testHeldOrderResume(const QString &dir)
{
    QDir().mkpath(dir + "/Data");
    QDir::setCurrent(dir);
    CHECK(writeFile("Data/Product.txt",
        "P1 100001 TEA Tea count 20.00 1.00 2.00 10 Data/barcodes/100001.png#\n"));
    CHECK(writeFile("Data/Order.txt", ""));
    CHECK(writeFile("Data/OrderDetail.txt", ""));

    QString error;
    OrderSession cashier;
    cashier.customerName = QStringLiteral("Sara Ahmed");
    cashier.customerContact = QStringLiteral("03001234567");
    CHECK(addProductToOrder(cashier, "100001", 2, error));
    CHECK(addProductToOrder(cashier, "100001", 3, error));
    CHECK(cashier.items.size() == 1 && cashier.items.first().quantity == 5);
    CHECK(holdOrder(cashier, error));
    const QString heldId = cashier.orderId;
    CHECK(!heldId.isEmpty() && cashier.items.first().quantity == 5);

    QVector<Order> orders;
    CHECK(readOrders("Data/Order.txt", orders, error));
    CHECK(orders.size() == 1 && orders.first().id == heldId
        && orders.first().status == QStringLiteral("on-hold"));
    QVector<Order> heldOrders;
    CHECK(readHeldOrders(heldOrders, error)
        && heldOrders.size() == 1 && heldOrders.first().id == heldId);
    QVector<OrderDetail> savedDetails;
    CHECK(readOrderDetails("Data/OrderDetail.txt", heldId, savedDetails, error));
    CHECK(savedDetails.size() == 1 && savedDetails.first().quantity == 5);

    // A fresh session models an app restart: resume reads the persistent files.
    OrderSession afterRestart;
    CHECK(resumeOrder(heldId, afterRestart, error));
    CHECK(afterRestart.orderId == heldId && afterRestart.items.size() == 1);
    CHECK(afterRestart.items.first().quantity == 5);
    CHECK(afterRestart.customerName == QStringLiteral("Sara_Ahmed"));

    Bill paidBill;
    CHECK(completePayment(afterRestart, "cash", QString(), 100,
        paidBill, error));
    CHECK(afterRestart.items.isEmpty() && afterRestart.orderId.isEmpty());
    CHECK(paidBill.totalPayable == 95 && paidBill.amountReceived == 100
        && paidBill.balanceReturned == 5);
    CHECK(paidBill.details.size() == 1
        && paidBill.details.first().totalDiscount == 10);
    CHECK(readOrders("Data/Order.txt", orders, error));
    CHECK(orders.size() == 1 && orders.first().id == heldId
        && orders.first().status == QStringLiteral("paid"));
    CHECK(readLines("Data/OrderDetail.txt").size() == 1);
    CHECK(readLines("Data/Bill.txt").size() == 1);
    CHECK(readLines("Data/BillDetail.txt")
        == QStringList({paidBill.id + " TEA 20.00 5 10.00#"}));
    CHECK(readLines("Data/Product.txt").first().contains(" 5 "));
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTemporaryDir tmp;
    CHECK(tmp.isValid());
    if (!tmp.isValid())
        return 1;

    testBillCalculations();
    testBillRepository(tmp.path() + "/repository");
    testCheckout(tmp.path() + "/checkout");
    testHeldOrderResume(tmp.path() + "/held");

    if (failures == 0)
        std::printf("All bill, checkout and held-order tests passed.\n");
    return failures == 0 ? 0 : 1;
}
