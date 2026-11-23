#include "../bill/bill_repository.h"
#include "../checkout/checkout_service.h"
#include "../common/data_paths.h"
#include "../order/order_repository.h"
#include "../order/order_service.h"
#include "../payment/payment_methods.h"
#include "../product/product_repository.h"
#include "../report/sales_report.h"
#include "../season/season_repository.h"
#include "../season/season_service.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <cstdio>

using namespace Pos;

static int failures = 0;
#define CHECK(condition) do { if (!(condition)) { \
    std::printf("FAIL line %d: %s\n", __LINE__, #condition); ++failures; } } while (0)

static Product makeProduct()
{
    Product product;
    product.id = QStringLiteral("P00001");
    product.barcode = QStringLiteral("100001");
    product.acronym = QStringLiteral("TEST");
    product.name = QStringLiteral("Test_product");
    product.unit = QStringLiteral("count");
    product.unitPrice = 100;
    product.tax = 10;
    product.discountPerUnit = 5;
    product.quantity = 5;
    product.barcodeImage = QStringLiteral("barcodes/100001.png");
    return product;
}

static void testSeasons()
{
    QVector<Season> seasons;
    QString error;
    CHECK(getAllSeasons(seasons, error));
    CHECK(seasons.isEmpty());

    Season active;
    active.name = QStringLiteral("TestSeason");
    active.startDate = QDate::currentDate().addDays(-1);
    active.endDate = QDate::currentDate().addDays(1);
    active.percentage = 10;
    active.isActive = true;
    CHECK(addSeason(active, error));
    QFile seasonFile(DataPaths::seasons());
    CHECK(seasonFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QByteArray seasonRecord = seasonFile.readLine().trimmed();
    CHECK(seasonRecord.endsWith('#'));
    CHECK(seasonRecord.count(' ') == 4);
    seasonFile.close();

    Season overlap = active;
    overlap.name = QStringLiteral("Overlap");
    CHECK(!addSeason(overlap, error));
    CHECK(error.contains(QStringLiteral("overlap"), Qt::CaseInsensitive));

    Season found;
    CHECK(findActiveSeason(QDate::currentDate(), found, error));
    CHECK(found.percentage == 10);
    CHECK(setSeasonActive(0, false, error));
    CHECK(!findActiveSeason(QDate::currentDate(), found, error));
    CHECK(error.isEmpty());
    CHECK(getAllSeasons(seasons, error) && seasons.size() == 1);
    CHECK(!seasons.first().isActive);
    CHECK(!addSeason(Season{}, error));
    CHECK(deleteSeason(0, error));
}

static void testPaymentValidation()
{
    QString error;
    CashPayment cash;
    CardPayment card;
    WalletPayment wallet;
    CHECK(cash.validate(120, 100, QString(), error));
    CHECK(!cash.validate(99.99, 100, QString(), error));
    CHECK(card.validate(100, 100, QStringLiteral("HBL"), error));
    CHECK(!card.validate(101, 100, QStringLiteral("HBL"), error));
    CHECK(!card.validate(100, 100, QString(), error));
    CHECK(wallet.validate(100, 100, QStringLiteral("JazzCash"), error));
}

static void testLegacyBillCompatibility(const QString &directory)
{
    const QString filePath = directory + QStringLiteral("/legacy_bill.txt");
    QFile file(filePath);
    CHECK(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("000000001 Ali_Raza 00000119 MLK1L 280.00 0.00 2 560.00 28.00 0.00 588.00 paid#\n");
    file.close();
    QVector<Bill> bills;
    QString error;
    CHECK(readAllBills(filePath, bills, error));
    CHECK(bills.size() == 1);
    CHECK(bills.first().totalPayable == 588);
    CHECK(bills.first().amountReceived == 0);
    CHECK(bills.first().balanceReturned == 0);
}

static void testCheckoutAndReports()
{
    const Product product = makeProduct();
    QString error;
    QVector<Product> products{product};
    CHECK(saveProducts(DataPaths::products(), products, error));

    Season active;
    active.name = QStringLiteral("CheckoutSeason");
    active.startDate = QDate::currentDate();
    active.endDate = QDate::currentDate();
    active.percentage = 10;
    active.isActive = true;
    CHECK(addSeason(active, error));

    const QVector<CartItem> items{{product, 1}, {product, 1}};
    CartTotals totals;
    CHECK(setSeasonActive(0, false, error));
    CHECK(calculateCheckoutTotals(items, 0, totals, error));
    CHECK(totals.seasonalDiscount == 0 && totals.payable == 210);
    CHECK(setSeasonActive(0, true, error));
    CHECK(calculateCheckoutTotals(items, 8, totals, error));
    CHECK(totals.withoutTax == 200.00);
    CHECK(totals.tax == 20.00);
    CHECK(totals.seasonalDiscount == 19.00);
    CHECK(totals.specialDiscount == 13.68);
    CHECK(totals.discount == 42.68);
    CHECK(totals.payable == 177.32);
    CHECK(!calculateCheckoutTotals(items, 8.01, totals, error));

    CheckoutRequest request;
    request.customerName = QStringLiteral("Test Customer");
    request.paymentMode = QStringLiteral("cash");
    request.amountPaid = 200;
    request.specialDiscountPercent = 8;
    request.items = items;
    CheckoutResult result;
    CHECK(completePayment(request, result, error));
    CHECK(result.order.status == QStringLiteral("paid"));
    CHECK(result.totals.payable == 177.32);
    CHECK(result.billRows.size() == 2);
    CHECK(result.billRows.first().amountReceived == 200);
    CHECK(result.billRows.first().balanceReturned == 22.68);

    QVector<Bill> savedRows;
    CHECK(readBill(DataPaths::bills(), result.billRows.first().id, savedRows, error));
    CHECK(savedRows.size() == 2);
    CHECK(savedRows.at(0).seasonalDiscount + savedRows.at(1).seasonalDiscount == 19);
    CHECK(savedRows.at(0).specialDiscount + savedRows.at(1).specialDiscount == 13.68);
    CHECK(savedRows.first().amountReceived == 200);
    CHECK(savedRows.first().balanceReturned == 22.68);

    SalesReport cashReport;
    SalesReport allReport;
    CHECK(cashSalesReport(QDate::currentDate(), QDate::currentDate(),
        cashReport, error));
    CHECK(cashReport.orderCount == 1 && cashReport.totalSales == 177.32);
    CHECK(allSalesReport(QDate::currentDate(), QDate::currentDate(),
        allReport, error));
    CHECK(allReport.totalDiscounts == 42.68);

    QVector<Order> orders;
    CHECK(readOrders(DataPaths::orders(), orders, error) && orders.size() == 1);
    QVector<OrderDetail> details;
    CHECK(readOrderDetails(DataPaths::orderDetail(), orders.first().id,
        details, error));
    CHECK(details.size() == 2);
    CHECK(details.at(0).quantity + details.at(1).quantity == 2);

    Order cardOrder;
    cardOrder.id = QStringLiteral("99112233");
    cardOrder.date = QDate::currentDate().toString(Qt::ISODate);
    cardOrder.time = QStringLiteral("12:00:00");
    cardOrder.customerName = QStringLiteral("Card Customer");
    cardOrder.status = QStringLiteral("paid");
    cardOrder.paymentAmount = 40;
    cardOrder.paymentMode = QStringLiteral("card");
    cardOrder.merchant = QStringLiteral("HBL");
    OrderDetail cardDetail;
    cardDetail.orderId = cardOrder.id;
    cardDetail.productId = product.id;
    cardDetail.barcode = product.barcode;
    cardDetail.quantity = 1;
    CHECK(saveOrder(DataPaths::orders(), DataPaths::orderDetail(),
        cardOrder, {cardDetail}, error));
    CHECK(cashSalesReport(QDate::currentDate(), QDate::currentDate(),
        cashReport, error));
    CHECK(cashReport.orderCount == 1 && cashReport.totalSales == 177.32);
    CHECK(allSalesReport(QDate::currentDate(), QDate::currentDate(),
        allReport, error));
    CHECK(allReport.orderCount == 2 && allReport.totalSales == 217.32);
    CHECK(allReport.totalDiscounts == 42.68);

    for (int i = 0; i < 5; ++i)
    {
        QDate from, to;
        const auto period = static_cast<SalesReportPeriod>(i);
        CHECK(reportDateRange(period, QDate::currentDate(),
            QDate::currentDate(), QDate::currentDate(), from, to, error));
        CHECK(cashSalesReport(from, to, cashReport, error));
        CHECK(cashReport.orderCount == 1 && cashReport.totalSales == 177.32);
    }

    QDate start, end;
    CHECK(reportDateRange(SalesReportPeriod::Weekly, QDate(2025, 1, 1),
        QDate(), QDate(), start, end, error));
    CHECK(start == QDate(2024, 12, 30) && end == QDate(2025, 1, 5));
    CHECK(reportDateRange(SalesReportPeriod::Daily, QDate(2025, 2, 4),
        QDate(), QDate(), start, end, error));
    CHECK(start == QDate(2025, 2, 4) && end == start);
    CHECK(reportDateRange(SalesReportPeriod::Monthly, QDate(2025, 2, 4),
        QDate(), QDate(), start, end, error));
    CHECK(start == QDate(2025, 2, 1) && end == QDate(2025, 2, 28));
    CHECK(reportDateRange(SalesReportPeriod::Yearly, QDate(2025, 2, 4),
        QDate(), QDate(), start, end, error));
    CHECK(start == QDate(2025, 1, 1) && end == QDate(2025, 12, 31));
    CHECK(reportDateRange(SalesReportPeriod::Custom, QDate(),
        QDate(2025, 2, 1), QDate(2025, 2, 28), start, end, error));
    CHECK(!reportDateRange(SalesReportPeriod::Custom, QDate(),
        QDate(2025, 3, 1), QDate(2025, 2, 28), start, end, error));
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    CHECK(temporary.isValid());
    QDir::setCurrent(temporary.path());
    CHECK(QDir().mkpath(QStringLiteral("Data")));

    testSeasons();
    testPaymentValidation();
    testLegacyBillCompatibility(temporary.path());
    testCheckoutAndReports();

    if (failures == 0)
        std::printf("All Anas task tests passed.\n");
    return failures == 0 ? 0 : 1;
}
