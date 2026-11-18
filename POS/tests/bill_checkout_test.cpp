// Owner: Ahad. Tests for bill repository, bill service and checkout.
// All file work happens in a temporary folder, never in the shared Data/.
#include "../bill/bill_repository.h"
#include "../bill/bill_service.h"
#include "../checkout/checkout_service.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
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
    Product p;
    p.id = id; p.barcode = barcode; p.acronym = acronym;
    p.unitPrice = price; p.tax = tax; p.discountPerUnit = discount;
    p.quantity = qty;
    return p;
}

static QStringList readLines(const QString &path)
{
    QStringList lines;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return lines;
    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (!line.trimmed().isEmpty())
            lines << line;
    }
    return lines;
}

static void testBillLines()
{
    Order order;
    order.id = "00000119"; order.customerName = "Ali Raza"; order.status = "paid";
    QString err;

    Bill milk, sugar;
    CHECK(calculateBillLine("000000001", order,
        makeProduct("P001", "100001", "MLK1L", 280, 14, 0, 50), 2, milk, err));
    CHECK(milk.totalWithoutTax == 560.00 && milk.totalTax == 28.00);
    CHECK(milk.totalDiscount == 0.00 && milk.totalPayable == 588.00);

    CHECK(calculateBillLine("000000001", order,
        makeProduct("P002", "100002", "SUGR1KG", 160, 8, 5, 40), 3, sugar, err));
    CHECK(sugar.totalPayable == 489.00 && sugar.totalDiscount == 15.00);

    Bill rounded;
    CHECK(calculateBillLine("000000001", order,
        makeProduct("P003", "100003", "ROUND", 1.006, 0.006, 0, 3),
        3, rounded, err));
    CHECK(rounded.unitPrice == 1.01 && rounded.totalWithoutTax == 3.03);
    CHECK(rounded.totalTax == 0.03 && rounded.totalPayable == 3.06);

    Bill bad;
    CHECK(!calculateBillLine("1", order, makeProduct("P", "1", "X", 1, 0, 0, 1), 0, bad, err));
    CHECK(!err.isEmpty());
    CHECK(!calculateBillLine("1", order, makeProduct("P", "1", "X", -1, 0, 0, 1), 1, bad, err));

    QVector<Bill> rows;
    rows.append(milk);
    CHECK(!buildBill("1", order, {}, rows, err));
    CHECK(rows.isEmpty());

    Product overDiscount = makeProduct("P", "1", "X", 10, 1, 20, 1);
    CHECK(!calculateBillLine("1", order, overDiscount, 1, bad, err));
    CHECK(bad.id.isEmpty());

    const QVector<CartItem> invalidItems = {
        { makeProduct("P001", "100001", "MLK1L", 280, 14, 0, 50), 2 },
        { makeProduct("P002", "100002", "SUGR1KG", 160, 8, 5, 40), 0 }
    };
    rows.append(milk);
    CHECK(!buildBill("000000001", order, invalidItems, rows, err));
    CHECK(rows.isEmpty());
}

static void testRepository(const QString &dir)
{
    const QString file = dir + "/Bill.txt";
    Order order;
    order.id = "00000119"; order.customerName = "Ali Raza"; order.status = "paid";
    const QVector<CartItem> items = {
        { makeProduct("P001", "100001", "MLK1L", 280, 14, 0, 50), 2 },
        { makeProduct("P002", "100002", "SUGR1KG", 160, 8, 5, 40), 3 }
    };
    QVector<Bill> rows;
    QString err;
    CHECK(buildBill("000000001", order, items, rows, err));
    CHECK(saveBill(file, rows, err));

    const QStringList lines = readLines(file);
    CHECK(lines.size() == 2);
    CHECK(lines.value(0) == "000000001 Ali_Raza 00000119 MLK1L 280.00 0.00 2 560.00 28.00 0.00 588.00 paid#");
    CHECK(lines.value(1) == "000000001 Ali_Raza 00000119 SUGR1KG 160.00 5.00 3 480.00 24.00 15.00 489.00 paid#");

    QVector<Bill> back;
    CHECK(readBill(file, "000000001", back, err));
    CHECK(back.size() == 2);
    CHECK(back.value(0).customerName == "Ali Raza");
    CHECK(back.value(1).totalPayable == 489.00);

    QVector<Bill> inconsistent = rows;
    inconsistent[1].orderId = QStringLiteral("00000120");
    CHECK(!saveBill(file, inconsistent, err));
    CHECK(readLines(file).size() == 2);
    inconsistent = rows;
    inconsistent[0].totalPayable += 1;
    CHECK(!saveBill(file, inconsistent, err));
    CHECK(readLines(file).size() == 2);

    // A second bill is appended; reading one bill returns only its own rows.
    QVector<Bill> second = rows;
    for (Bill &b : second) b.id = "000000002";
    CHECK(saveBill(file, second, err));
    CHECK(readLines(file).size() == 4);
    CHECK(readBill(file, "000000002", back, err) && back.size() == 2);

    // Error cases: unknown bill, missing file, bad line, empty bill.
    CHECK(!readBill(file, "999999999", back, err) && !err.isEmpty());
    CHECK(!readBill(dir + "/missing.txt", "1", back, err));
    CHECK(!saveBill(file, {}, err));

    // A file whose last line has no newline must not get two bills glued together.
    QFile noNl(dir + "/NoNewline.txt");
    CHECK(noNl.open(QIODevice::WriteOnly | QIODevice::Text));
    noNl.write(lines.value(0).toUtf8());   // one line, no trailing newline
    noNl.close();
    CHECK(saveBill(dir + "/NoNewline.txt", second, err));
    CHECK(readLines(dir + "/NoNewline.txt").size() == 3);

    QFile bad(dir + "/Bad.txt");
    CHECK(bad.open(QIODevice::WriteOnly | QIODevice::Text));
    bad.write("this is not a bill line\n");
    bad.close();
    CHECK(!readBill(dir + "/Bad.txt", "1", back, err));
}

static void testCheckout(const QString &dir)
{
    QDir().mkpath(dir + "/Data");
    QDir::setCurrent(dir);   // DataPaths are relative to the working folder

    CheckoutRequest request;
    request.customerName = "Ali Raza";
    request.customerContact = "03001234567";
    request.paymentMode = "wallet";
    request.merchant = "JazzCash";
    request.amountPaid = 1077.00;
    request.items = {
        { makeProduct("P001", "100001", "MLK1L", 280, 14, 0, 50), 2 },
        { makeProduct("P002", "100002", "SUGR1KG", 160, 8, 5, 40), 3 }
    };

    // Empty cart and missing merchant must not write anything.
    CheckoutResult result;
    result.totals.payable = 5;
    QString err;
    CheckoutRequest empty = request;
    empty.items.clear();
    CHECK(!generateBill(empty, result, err) && !err.isEmpty());
    CHECK(result.totals.payable == 0 && result.order.id.isEmpty());

    CheckoutRequest noMerchant = request;
    noMerchant.merchant.clear();
    CHECK(!generateBill(noMerchant, result, err) && !err.isEmpty());
    CHECK(readLines("Data/Order.txt").isEmpty());
    CHECK(readLines("Data/Bill.txt").isEmpty());

    CHECK(generateBill(request, result, err));
    CHECK(result.totals.payable == 1077.00);
    CHECK(result.order.status == "paid");
    CHECK(result.details.size() == 2 && result.billRows.size() == 2);

    // The three output files, line by line.
    const QStringList orders = readLines("Data/Order.txt");
    CHECK(orders.size() == 1);
    CHECK(orders.value(0).contains("Ali_Raza 03001234567 paid 1077.00 wallet JazzCash#"));
    CHECK(orders.value(0).startsWith(result.order.id + " "));

    const QStringList details = readLines("Data/OrderDetail.txt");
    CHECK(details.size() == 2);
    CHECK(details.value(0) == result.order.id + " P001 100001 2#");
    CHECK(details.value(1) == result.order.id + " P002 100002 3#");

    const QStringList bills = readLines("Data/Bill.txt");
    CHECK(bills.size() == 2);
    CHECK(bills.value(0).endsWith("MLK1L 280.00 0.00 2 560.00 28.00 0.00 588.00 paid#"));
    CHECK(bills.value(1).endsWith("SUGR1KG 160.00 5.00 3 480.00 24.00 15.00 489.00 paid#"));
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTemporaryDir tmp;
    CHECK(tmp.isValid());

    testBillLines();
    testRepository(tmp.path());
    testCheckout(tmp.path() + "/checkout");

    if (failures == 0)
        std::printf("All bill and checkout tests passed.\n");
    return failures == 0 ? 0 : 1;
}
