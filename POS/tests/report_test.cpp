// Owner: Umar Khan - tests for the two manager reports (Increment 3, req 11):
// top 10 most sold items and top 10 items by remaining stock. Edge cases on
// made-up data in a temporary folder, then rules that must hold on the
// project's real sample data.

#include "../report/report_service.h"

#include <QCoreApplication>
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

    if (failures > 0)
    {
        QTextStream(stderr) << failures << " check(s) failed." << Qt::endl;
        return 1;
    }
    QTextStream(stdout) << "Report test passed." << Qt::endl;
    return 0;
}
