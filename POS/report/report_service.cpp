#include "report_service.h"

#include "../bill/bill_repository.h"
#include "../order/order_repository.h"
#include "../product/product_repository.h"

#include <QFileInfo>
#include <QHash>

#include <algorithm>
#include <cmath>
#include <functional>

namespace Pos
{
namespace
{
// Name first (case-insensitive), then id: a fixed order for equal numbers.
bool comesFirstByName(const Product &a, const Product &b)
{
    const int byName = a.name.compare(b.name, Qt::CaseInsensitive);
    if (byName != 0)
        return byName < 0;
    return a.id < b.id;
}
}

namespace
{
double round2(double value)
{
    return std::round(value * 100.0) / 100.0;
}

// Paid orders' bills that pass `keep`, oldest first, with their totals.
bool collectSales(const QString &orderFile, const QString &billFile,
    const std::function<bool(const Bill &, const QString &paymentMode)> &keep,
    SalesReport &report, QString &error)
{
    report = SalesReport{};
    QVector<Order> orders;
    if (QFileInfo::exists(orderFile) && !readOrders(orderFile, orders, error))
        return false;
    QHash<QString, QString> paidOrderMode;   // order id -> payment mode
    for (const Order &order : orders)
        if (order.status == QStringLiteral("paid"))
            paidOrderMode.insert(order.id, order.paymentMode);

    QVector<Bill> bills;
    if (!readBills(billFile, bills, error))
        return false;
    for (const Bill &bill : bills)
    {
        if (!paidOrderMode.contains(bill.orderId))
            continue;
        const QString mode = paidOrderMode.value(bill.orderId);
        if (keep(bill, mode))
            report.sales.append(Sale{bill, mode});
    }
    std::sort(report.sales.begin(), report.sales.end(), [](const Sale &a, const Sale &b) {
        if (a.bill.date != b.bill.date)
            return a.bill.date < b.bill.date;   // yyyy-MM-dd sorts by text
        if (a.bill.time != b.bill.time)
            return a.bill.time < b.bill.time;
        return a.bill.id < b.bill.id;
    });
    for (const Sale &sale : report.sales)
    {
        report.totalWithoutTax += sale.bill.totalWithoutTax;
        report.totalTax += sale.bill.totalTax;
        report.productDiscounts += sale.bill.totalDiscount;
        report.seasonalDiscounts += sale.bill.seasonalDiscount;
        report.specialDiscounts += sale.bill.specialDiscount;
        report.totalAmount += sale.bill.totalPayable;
    }
    report.totalWithoutTax = round2(report.totalWithoutTax);
    report.totalTax = round2(report.totalTax);
    report.productDiscounts = round2(report.productDiscounts);
    report.seasonalDiscounts = round2(report.seasonalDiscounts);
    report.specialDiscounts = round2(report.specialDiscounts);
    report.totalDiscounts = round2(report.productDiscounts + report.seasonalDiscounts
        + report.specialDiscounts);
    report.totalAmount = round2(report.totalAmount);
    error.clear();
    return true;
}
}

void salesPeriodRange(SalesPeriod period, QDate date, QDate &from, QDate &to)
{
    switch (period)
    {
    case SalesPeriod::Daily:
        from = to = date;
        break;
    case SalesPeriod::Weekly:   // Monday to Sunday
        from = date.addDays(1 - date.dayOfWeek());
        to = from.addDays(6);
        break;
    case SalesPeriod::Monthly:
        from = QDate(date.year(), date.month(), 1);
        to = from.addMonths(1).addDays(-1);
        break;
    case SalesPeriod::Yearly:
        from = QDate(date.year(), 1, 1);
        to = QDate(date.year(), 12, 31);
        break;
    }
}

bool cashSalesReport(QString orderFile, QString billFile, QDate from, QDate to,
    SalesReport &report, QString &error)
{
    report = SalesReport{};
    if (!from.isValid() || !to.isValid() || from > to)
    {
        error = QStringLiteral("Choose a period whose start date is on or before its end date.");
        return false;
    }
    return collectSales(orderFile, billFile,
        [from, to](const Bill &bill, const QString &paymentMode) {
            const QDate date = QDate::fromString(bill.date, Qt::ISODate);
            return paymentMode == QStringLiteral("cash") && date >= from && date <= to;
        },
        report, error);
}

bool allSalesReport(QString orderFile, QString billFile, SalesReport &report, QString &error)
{
    return collectSales(orderFile, billFile,
        [](const Bill &, const QString &) { return true; }, report, error);
}


bool topSoldProducts(QString productFile, QString orderFile, QString orderDetailFile,
    int limit, QVector<SoldItem> &rows, QString &error, QString unitFilter)
{
    rows.clear();
    QVector<Product> products;
    if (!readProducts(productFile, products, error))
        return false;

    QVector<Order> orders;
    if (QFileInfo::exists(orderFile) && !readOrders(orderFile, orders, error))
        return false;

    QHash<QString, int> sold;           // product id -> quantity sold
    QHash<QString, QString> barcodeOf;  // for products no longer in Product.txt
    if (QFileInfo::exists(orderDetailFile))
    {
        for (const Order &order : orders)
        {
            if (order.status != QStringLiteral("paid"))
                continue;
            QVector<OrderDetail> details;
            if (!readOrderDetails(orderDetailFile, order.id, details, error))
                return false;
            for (const OrderDetail &detail : details)
            {
                sold[detail.productId] += detail.quantity;
                barcodeOf.insert(detail.productId, detail.barcode);
            }
        }
    }

    QVector<SoldItem> all;
    for (auto it = sold.cbegin(); it != sold.cend(); ++it)
    {
        if (it.value() <= 0)
            continue;
        SoldItem item;
        item.quantitySold = it.value();
        const auto found = std::find_if(products.cbegin(), products.cend(),
            [&](const Product &p) { return p.id == it.key(); });
        if (found != products.cend())
            item.product = *found;
        else
        {
            item.inProductFile = false;
            item.product.id = it.key();
            item.product.name = it.key();
            item.product.barcode = barcodeOf.value(it.key());
        }
        // A product no longer in Product.txt has no known unit: only in "all".
        if (!unitFilter.isEmpty() && (!item.inProductFile || item.product.unit != unitFilter))
            continue;
        all.append(item);
    }

    std::sort(all.begin(), all.end(), [](const SoldItem &a, const SoldItem &b) {
        if (a.quantitySold != b.quantitySold)
            return a.quantitySold > b.quantitySold;
        return comesFirstByName(a.product, b.product);
    });
    rows = all.mid(0, qMax(0, limit));
    error.clear();
    return true;
}

bool topStockProducts(QString productFile, int limit, QVector<Product> &rows, QString &error,
    QString unitFilter)
{
    rows.clear();
    QVector<Product> products;
    if (!readProducts(productFile, products, error))
        return false;
    if (!unitFilter.isEmpty())
        products.erase(std::remove_if(products.begin(), products.end(),
            [&](const Product &p) { return p.unit != unitFilter; }), products.end());
    std::sort(products.begin(), products.end(), [](const Product &a, const Product &b) {
        if (a.quantity != b.quantity)
            return a.quantity > b.quantity;
        return comesFirstByName(a, b);
    });
    rows = products.mid(0, qMax(0, limit));
    error.clear();
    return true;
}
}
