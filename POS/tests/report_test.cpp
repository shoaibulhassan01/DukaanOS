// Owner: Umar Khan - tests for the manager reports (Increment 3, req 11):
// top 10 most sold items, top 10 items by remaining stock, cash sales by
// period and all sales with totals and discounts. Edge cases on
// made-up data in a temporary folder, then rules that must hold on the
// project's real sample data.

#include "../product/product_service.h"
#include "../report/report_service.h"

#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

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

void writeText(const QString &path, const QString &text)
{
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        file.write(text.toUtf8());
}

// One Product.txt line: id, name, quantity (barcode from the id number).
QString product(int number, const QString &name, int quantity)
{
    const QString barcode = QStringLiteral("10%1").arg(number, 4, 10, QLatin1Char('0'));
    return QStringLiteral("P%1 %2 A%1 %3 count 10.00 0.00 0.00 %4 Data/barcodes/%2.png 10#\n")
        .arg(number, 4, 10, QLatin1Char('0')).arg(barcode, name).arg(quantity);
}

QString order(const QString &id, const QString &status)
{
    return QStringLiteral("%1 2026-11-23 10:00:00 Walk-in - %2 10.00 cash -#\n").arg(id, status);
}

QString item(const QString &orderId, int number, int quantity)
{
    return QStringLiteral("%1 P%2 10%2 %3#\n").arg(orderId).arg(number, 4, 10, QLatin1Char('0')).arg(quantity);
}

QStringList names(const QVector<Pos::SoldItem> &rows)
{
    QStringList list;
    for (const Pos::SoldItem &row : rows)
        list << QStringLiteral("%1:%2").arg(row.product.name).arg(row.quantitySold);
    return list;
}

QStringList names(const QVector<Pos::Product> &rows)
{
    QStringList list;
    for (const Pos::Product &row : rows)
        list << QStringLiteral("%1:%2").arg(row.name).arg(row.quantity);
    return list;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    if (!dir.isValid())
        return 1;
    const QString products = dir.filePath(QStringLiteral("Product.txt"));
    const QString orders = dir.filePath(QStringLiteral("Order.txt"));
    const QString details = dir.filePath(QStringLiteral("OrderDetail.txt"));
    QString error;
    QVector<Pos::SoldItem> sold;
    QVector<Pos::Product> stock;

    // ------------------------------------------------------- missing files
    check(!Pos::topSoldProducts(products, orders, details, 10, sold, error),
        QStringLiteral("Top sold must report a missing Product.txt."));
    check(!Pos::topStockProducts(products, 10, stock, error),
        QStringLiteral("Top stock must report a missing Product.txt."));

    // Three products, no orders yet.
    writeText(products, product(1, QStringLiteral("Rice"), 100) + product(2, QStringLiteral("Tea"), 5) +
        product(3, QStringLiteral("Apple"), 5));
    check(Pos::topSoldProducts(products, orders, details, 10, sold, error) && sold.isEmpty(),
        QStringLiteral("With no Order.txt nothing has been sold (empty report, not an error): %1").arg(error));
    check(Pos::topStockProducts(products, 10, stock, error) &&
            names(stock) == QStringList({QStringLiteral("Rice:100"), QStringLiteral("Apple:5"), QStringLiteral("Tea:5")}),
        QStringLiteral("Fewer than 10 products: all shown, highest stock first, ties by name. Got: %1")
            .arg(names(stock).join(QStringLiteral(", "))));
    writeText(products, QString());
    check(Pos::topStockProducts(products, 10, stock, error) && stock.isEmpty(),
        QStringLiteral("An empty Product.txt gives an empty stock report."));

    // ----------------------------------------------- 12 products, 12 sold
    QString productText;
    const QStringList productNames = {QStringLiteral("Apple"), QStringLiteral("Bread"), QStringLiteral("Chips"),
        QStringLiteral("Daal"), QStringLiteral("Eggs"), QStringLiteral("Flour"), QStringLiteral("Ghee"),
        QStringLiteral("Honey"), QStringLiteral("Ice"), QStringLiteral("Jam"), QStringLiteral("Ketchup"),
        QStringLiteral("Lentils")};
    for (int i = 0; i < productNames.size(); ++i)
        productText += product(i + 1, productNames.at(i), (i + 1) * 10 % 70);   // stock 10..60, 0, 10..50
    writeText(products, productText);

    // Product i is sold i times in paid orders 1 and 2 together; on-hold and
    // archived orders sell a lot of Apple, which must NOT count.
    QString orderText = order(QStringLiteral("00000001"), QStringLiteral("paid")) +
        order(QStringLiteral("00000002"), QStringLiteral("paid")) +
        order(QStringLiteral("00000003"), QStringLiteral("on-hold")) +
        order(QStringLiteral("00000004"), QStringLiteral("archived"));
    QString detailText;
    for (int i = 1; i <= 12; ++i)
    {
        const int first = i / 2;
        detailText += item(QStringLiteral("00000001"), i, first > 0 ? first : 1);
        if (i - (first > 0 ? first : 1) > 0)
            detailText += item(QStringLiteral("00000002"), i, i - (first > 0 ? first : 1));
    }
    detailText += item(QStringLiteral("00000002"), 1, 1);   // Apple: 2 in total, a tie with Bread
    detailText += item(QStringLiteral("00000003"), 1, 500) + item(QStringLiteral("00000004"), 1, 900);
    writeText(orders, orderText);
    writeText(details, detailText);

    check(Pos::topSoldProducts(products, orders, details, 10, sold, error) && sold.size() == 10,
        QStringLiteral("12 products sold: the report must show exactly 10, got %1 (%2).").arg(sold.size()).arg(error));
    check(names(sold).value(0) == QStringLiteral("Lentils:12") && names(sold).value(9) == QStringLiteral("Chips:3"),
        QStringLiteral("Most sold first, quantities added up over orders. Got: %1").arg(names(sold).join(QStringLiteral(", "))));
    bool descending = true;
    for (int i = 1; i < sold.size(); ++i)
        descending = descending && sold.at(i - 1).quantitySold >= sold.at(i).quantitySold;
    check(descending, QStringLiteral("Top sold must be in descending order."));

    check(sold.value(0).product.quantity == 50 && sold.value(0).inProductFile,
        QStringLiteral("Top sold must show the remaining stock from Product.txt (Lentils 50)."));

    // Ties: Apple and Bread both sold 2 -> ordered by name. Apple's 500 + 900
    // in held and archived orders must not count.
    check(Pos::topSoldProducts(products, orders, details, 12, sold, error) &&
            names(sold).mid(10) == QStringList({QStringLiteral("Apple:2"), QStringLiteral("Bread:2")}),
        QStringLiteral("Equal quantities sold must be ordered by name. Got: %1").arg(names(sold).join(QStringLiteral(", "))));
    check(Pos::topSoldProducts(products, orders, details, 0, sold, error) && sold.isEmpty(),
        QStringLiteral("A limit of 0 gives no rows."));

    // Top stock: 10 rows, descending, ties by name, a product with 0 stock last.
    check(Pos::topStockProducts(products, 10, stock, error) && stock.size() == 10,
        QStringLiteral("12 products: the stock report must show exactly 10."));
    check(names(stock).mid(0, 3) == QStringList({QStringLiteral("Flour:60"), QStringLiteral("Eggs:50"),
              QStringLiteral("Lentils:50")}),
        QStringLiteral("Highest stock first, ties by name. Got: %1").arg(names(stock).join(QStringLiteral(", "))));
    check(Pos::topStockProducts(products, 12, stock, error) && names(stock).last() == QStringLiteral("Ghee:0"),
        QStringLiteral("A product with 0 stock comes last. Got: %1").arg(names(stock).join(QStringLiteral(", "))));

    // A sold product that was later removed from Product.txt still counts.
    writeText(details, detailText + item(QStringLiteral("00000001"), 77, 40));
    check(Pos::topSoldProducts(products, orders, details, 10, sold, error) &&
            sold.value(0).product.id == QStringLiteral("P0077") && !sold.value(0).inProductFile,
        QStringLiteral("A sold product missing from Product.txt must still be listed (marked as missing)."));

    // A broken Order.txt is an error, not an empty report.
    writeText(orders, QStringLiteral("00000001 broken#\n"));
    check(!Pos::topSoldProducts(products, orders, details, 10, sold, error) && !error.isEmpty(),
        QStringLiteral("A broken Order.txt must be reported."));

    // ------------------------------------------------- unit filter
    // Counts and kilograms are ranked separately when a unit is chosen; the
    // filter is applied before the top ten is taken.
    writeText(orders, orderText);
    QString mixed = productText;
    mixed += QStringLiteral("P0090 100090 RCE Rice weight 10.00 0.00 0.00 80 Data/barcodes/100090.png 10#\n");
    mixed += QStringLiteral("P0091 100091 SGR Sugar weight 10.00 0.00 0.00 5 Data/barcodes/100091.png 10#\n");
    writeText(products, mixed);
    writeText(details, detailText + item(QStringLiteral("00000001"), 90, 1) + item(QStringLiteral("00000002"), 91, 2) +
        item(QStringLiteral("00000001"), 77, 40));
    check(Pos::topSoldProducts(products, orders, details, 10, sold, error, QStringLiteral("weight")) &&
            names(sold) == QStringList({QStringLiteral("Sugar:2"), QStringLiteral("Rice:1")}),
        QStringLiteral("Weighed items only: Sugar 2, Rice 1 (they are below the overall top ten). Got: %1")
            .arg(names(sold).join(QStringLiteral(", "))));
    check(Pos::topSoldProducts(products, orders, details, 10, sold, error, QStringLiteral("count")) &&
            sold.size() == 10 && names(sold).value(0) == QStringLiteral("Lentils:12"),
        QStringLiteral("Counted items only: 10 rows starting with Lentils; the removed product P0077 has no unit "
                       "and is left out. Got: %1").arg(names(sold).join(QStringLiteral(", "))));
    check(Pos::topSoldProducts(products, orders, details, 10, sold, error) && sold.value(0).product.id == QStringLiteral("P0077"),
        QStringLiteral("All items: the removed product P0077 is still listed."));
    check(Pos::topStockProducts(products, 10, stock, error, QStringLiteral("weight")) &&
            names(stock) == QStringList({QStringLiteral("Rice:80"), QStringLiteral("Sugar:5")}),
        QStringLiteral("Stock, weighed items only: Rice 80, Sugar 5. Got: %1").arg(names(stock).join(QStringLiteral(", "))));
    check(Pos::topStockProducts(products, 10, stock, error) && names(stock).value(0) == QStringLiteral("Rice:80"),
        QStringLiteral("Stock, all items: Rice (80) is first."));
    check(Pos::topStockProducts(products, 10, stock, error, QStringLiteral("count")) &&
            stock.size() == 10 && !names(stock).contains(QStringLiteral("Rice:80")),
        QStringLiteral("Stock, counted items only: 10 rows, no weighed items."));

    // How quantities are shown: weight is in kilograms.
    check(Pos::unitLabel(QStringLiteral("weight")) == QStringLiteral("weight (kg)") &&
            Pos::unitLabel(QStringLiteral("count")) == QStringLiteral("count"),
        QStringLiteral("Units must show as 'count' and 'weight (kg)'."));
    check(Pos::quantityText(100, QStringLiteral("weight")) == QStringLiteral("100 kg") &&
            Pos::quantityText(36, QStringLiteral("count")) == QStringLiteral("36 count"),
        QStringLiteral("Quantities must show as '100 kg' and '36 count'."));

    // ------------------------------------------- sales reports (req 11)
    // Calendar periods around a chosen date (week = Monday to Sunday).
    QDate from;
    QDate to;
    const QDate wednesday(2026, 11, 25);
    Pos::salesPeriodRange(Pos::SalesPeriod::Daily, wednesday, from, to);
    check(from == wednesday && to == wednesday, QStringLiteral("Daily period must be the chosen day."));
    Pos::salesPeriodRange(Pos::SalesPeriod::Weekly, wednesday, from, to);
    check(from == QDate(2026, 11, 23) && to == QDate(2026, 11, 29),
        QStringLiteral("Weekly period must be Monday to Sunday, got %1 to %2.")
            .arg(from.toString(Qt::ISODate), to.toString(Qt::ISODate)));
    Pos::salesPeriodRange(Pos::SalesPeriod::Weekly, QDate(2026, 11, 29), from, to);
    check(from == QDate(2026, 11, 23) && to == QDate(2026, 11, 29),
        QStringLiteral("A Sunday belongs to the week that started on Monday."));
    Pos::salesPeriodRange(Pos::SalesPeriod::Monthly, QDate(2028, 2, 10), from, to);
    check(from == QDate(2028, 2, 1) && to == QDate(2028, 2, 29),
        QStringLiteral("Monthly period must be the whole month (leap February)."));
    Pos::salesPeriodRange(Pos::SalesPeriod::Yearly, wednesday, from, to);
    check(from == QDate(2026, 1, 1) && to == QDate(2026, 12, 31),
        QStringLiteral("Yearly period must be 1 January to 31 December."));

    // Paid cash/card orders with bills, an on-hold order (no bill) and an
    // archived (discarded) order that still has a bill line.
    const QString salesOrders = dir.filePath(QStringLiteral("SalesOrder.txt"));
    const QString salesBills = dir.filePath(QStringLiteral("SalesBill.txt"));
    writeText(salesOrders,
        QStringLiteral("00000001 2026-11-23 10:00:00 A - paid 95.00 cash -#\n"
                       "00000002 2026-11-25 11:00:00 B - paid 200.00 card HBL#\n"
                       "00000003 2026-11-29 18:00:00 C - paid 47.50 cash -#\n"
                       "00000004 2026-11-30 09:00:00 D - paid 30.00 cash -#\n"
                       "00000005 2026-12-01 12:00:00 E - paid 42.00 cash -#\n"
                       "00000006 2025-11-25 12:00:00 F - paid 63.00 cash -#\n"
                       "00000007 2026-11-24 12:00:00 H - on-hold 0.00 cash -#\n"
                       "00000008 2026-11-24 12:00:00 G - archived 0.00 cash -#\n"));
    writeText(salesBills,
        QStringLiteral("261123001 A 00000001 100.00 5.00 10.00 95.00 100.00 5.00 paid 0.00 0.00 2026-11-23 10:00:00#\n"
                       "261125001 B 00000002 200.00 10.00 0.00 200.00 200.00 0.00 paid 0.00 10.00 2026-11-25 11:00:00#\n"
                       "261129001 C 00000003 50.00 2.50 0.00 47.50 50.00 2.50 paid 5.00 0.00 2026-11-29 18:00:00#\n"
                       "261130001 D 00000004 30.00 1.50 1.50 30.00 30.00 0.00 paid 0.00 0.00 2026-11-30 09:00:00#\n"
                       "261201001 E 00000005 40.00 2.00 0.00 42.00 50.00 8.00 paid 0.00 0.00 2026-12-01 12:00:00#\n"
                       "251125001 F 00000006 60.00 3.00 0.00 63.00 63.00 0.00 paid 0.00 0.00 2025-11-25 12:00:00#\n"
                       "261124001 G 00000008 70.00 3.50 0.00 73.50 0.00 0.00 archived 0.00 0.00 2026-11-24 12:00:00#\n"));
    auto billIds = [](const Pos::SalesReport &report) {
        QStringList ids;
        for (const Pos::Sale &sale : report.sales)
            ids << sale.bill.id;
        return ids.join(QLatin1Char(','));
    };
    Pos::SalesReport sales;
    Pos::salesPeriodRange(Pos::SalesPeriod::Weekly, wednesday, from, to);
    check(Pos::cashSalesReport(salesOrders, salesBills, from, to, sales, error)
            && billIds(sales) == QStringLiteral("261123001,261129001") && sales.totalAmount == 142.50,
        QStringLiteral("Weekly cash sales: only paid cash bills of that week (card, on-hold and archived "
                       "excluded); got %1 total %2 %3").arg(billIds(sales)).arg(sales.totalAmount).arg(error));
    check(Pos::cashSalesReport(salesOrders, salesBills, wednesday, wednesday, sales, error)
            && sales.sales.isEmpty() && sales.totalAmount == 0,
        QStringLiteral("Daily cash sales: a day with only a card sale must be empty."));
    Pos::salesPeriodRange(Pos::SalesPeriod::Monthly, wednesday, from, to);
    check(Pos::cashSalesReport(salesOrders, salesBills, from, to, sales, error)
            && billIds(sales) == QStringLiteral("261123001,261129001,261130001") && sales.totalAmount == 172.50,
        QStringLiteral("Monthly cash sales gave %1 total %2").arg(billIds(sales)).arg(sales.totalAmount));
    Pos::salesPeriodRange(Pos::SalesPeriod::Yearly, wednesday, from, to);
    check(Pos::cashSalesReport(salesOrders, salesBills, from, to, sales, error)
            && sales.sales.size() == 4 && sales.totalAmount == 214.50,
        QStringLiteral("Yearly cash sales gave %1 total %2").arg(billIds(sales)).arg(sales.totalAmount));
    check(Pos::cashSalesReport(salesOrders, salesBills, QDate(2025, 11, 1), QDate(2026, 11, 23), sales, error)
            && billIds(sales) == QStringLiteral("251125001,261123001") && sales.totalAmount == 158.00,
        QStringLiteral("Specified period: both end days included, oldest first; got %1").arg(billIds(sales)));
    check(!Pos::cashSalesReport(salesOrders, salesBills, QDate(2026, 11, 30), QDate(2026, 11, 1), sales, error)
            && !error.isEmpty(),
        QStringLiteral("A period that ends before it starts must be refused."));

    check(Pos::allSalesReport(salesOrders, salesBills, sales, error)
            && billIds(sales) == QStringLiteral("251125001,261123001,261125001,261129001,261130001,261201001"),
        QStringLiteral("All sales: every paid bill (any payment), oldest first; got %1 %2").arg(billIds(sales), error));
    check(sales.totalAmount == 477.50 && sales.totalWithoutTax == 480.00 && sales.totalTax == 24.00,
        QStringLiteral("All sales totals wrong: amount %1, before tax %2, tax %3")
            .arg(sales.totalAmount).arg(sales.totalWithoutTax).arg(sales.totalTax));
    check(sales.productDiscounts == 11.50 && sales.seasonalDiscounts == 5.00
            && sales.specialDiscounts == 10.00 && sales.totalDiscounts == 26.50,
        QStringLiteral("All sales discounts wrong: product %1, seasonal %2, special %3, total %4")
            .arg(sales.productDiscounts).arg(sales.seasonalDiscounts)
            .arg(sales.specialDiscounts).arg(sales.totalDiscounts));
    check(Pos::allSalesReport(dir.filePath(QStringLiteral("NoOrders.txt")),
              dir.filePath(QStringLiteral("NoBills.txt")), sales, error) && sales.sales.isEmpty(),
        QStringLiteral("No Order.txt / Bill.txt yet must mean no sales, not an error: %1").arg(error));

    // ------------------------------------------- the real sample data
    const QDir data(QDir(QStringLiteral(DUKAANOS_SOURCE_DIR)).filePath(QStringLiteral("Data")));
    check(Pos::topSoldProducts(data.filePath(QStringLiteral("Product.txt")), data.filePath(QStringLiteral("Order.txt")),
              data.filePath(QStringLiteral("OrderDetail.txt")), 10, sold, error),
        QStringLiteral("Top sold fails on the sample data: %1").arg(error));
    check(!sold.isEmpty() && sold.size() <= 10, QStringLiteral("Sample data: 1 to 10 sold items expected."));
    for (int i = 1; i < sold.size(); ++i)
        check(sold.at(i - 1).quantitySold >= sold.at(i).quantitySold,
            QStringLiteral("Sample data: top sold is not in descending order."));
    check(Pos::topStockProducts(data.filePath(QStringLiteral("Product.txt")), 10, stock, error) &&
            stock.size() == qMin(10, stock.size()) && !stock.isEmpty(),
        QStringLiteral("Top stock fails on the sample data: %1").arg(error));
    for (int i = 1; i < stock.size(); ++i)
        check(stock.at(i - 1).quantity >= stock.at(i).quantity,
            QStringLiteral("Sample data: top stock is not in descending order."));
    check(Pos::allSalesReport(data.filePath(QStringLiteral("Order.txt")), data.filePath(QStringLiteral("Bill.txt")),
              sales, error) && !sales.sales.isEmpty(),
        QStringLiteral("All sales fails on the sample data: %1").arg(error));
    double sampleTotal = 0;
    for (const Pos::Sale &sale : sales.sales)
        sampleTotal += sale.bill.totalPayable;
    check(qAbs(sampleTotal - sales.totalAmount) < 0.005,
        QStringLiteral("Sample data: the all-sales total must equal the sum of its rows."));

    if (failures > 0)
    {
        QTextStream(stderr) << failures << " check(s) failed." << Qt::endl;
        return 1;
    }
    QTextStream(stdout) << "Report test passed." << Qt::endl;
    return 0;
}
