#include "sales_report.h"

#include "../bill/bill_repository.h"
#include "../common/data_paths.h"
#include "../order/order_repository.h"

#include <cmath>
#include <QFile>
#include <QHash>

namespace Pos
{
namespace
{
double round2(double amount)
{
    return std::round(amount * 100.0) / 100.0;
}

bool loadOrders(QVector<Order> &orders, QString &error)
{
    return readOrders(DataPaths::orders(), orders, error);
}

bool loadDiscounts(QHash<QString, double> &discounts, QString &error)
{
    discounts.clear();
    const QString path = DataPaths::bills();
    if (!QFile::exists(path))
        return true;
    QVector<Bill> rows;
    if (!readAllBills(path, rows, error))
        return false;
    for (const Bill &bill : rows)
        discounts[bill.orderId] += bill.totalDiscount
            + bill.seasonalDiscount + bill.specialDiscount;
    for (double &discount : discounts)
        discount = round2(discount);
    return true;
}

bool validRange(QDate start, QDate end, QString &error)
{
    if (!start.isValid() || !end.isValid() || start > end)
    {
        error = QStringLiteral("Select a valid report date range.");
        return false;
    }
    return true;
}

bool generateReport(QDate start, QDate end, bool cashOnly,
    SalesReport &report, QString &error)
{
    report = SalesReport{};
    if (!validRange(start, end, error))
        return false;
    QVector<Order> orders;
    if (!loadOrders(orders, error))
        return false;
    QHash<QString, double> discounts;
    if (!cashOnly && !loadDiscounts(discounts, error))
        return false;

    SalesReport result;
    result.startDate = start;
    result.endDate = end;
    for (const Order &order : orders)
    {
        if (order.status != QStringLiteral("paid")
            || (cashOnly && order.paymentMode != QStringLiteral("cash")))
            continue;
        const QDate orderDate = QDate::fromString(order.date, Qt::ISODate);
        if (!orderDate.isValid())
        {
            error = QStringLiteral("Order %1 has an invalid date.").arg(order.id);
            return false;
        }
        if (orderDate < start || orderDate > end)
            continue;
        if (!std::isfinite(order.paymentAmount) || order.paymentAmount < 0)
        {
            error = QStringLiteral("Order %1 has an invalid sales amount.").arg(order.id);
            return false;
        }
        ++result.orderCount;
        result.totalSales += order.paymentAmount;
        if (!cashOnly)
            result.totalDiscounts += discounts.value(order.id);
    }
    result.totalSales = round2(result.totalSales);
    result.totalDiscounts = round2(result.totalDiscounts);
    report = result;
    error.clear();
    return true;
}
}

bool reportDateRange(SalesReportPeriod period, QDate anchor,
    QDate customStart, QDate customEnd, QDate &start, QDate &end,
    QString &error)
{
    error.clear();
    if (!anchor.isValid() && period != SalesReportPeriod::Custom)
    {
        error = QStringLiteral("The report date is invalid.");
        return false;
    }
    switch (period)
    {
    case SalesReportPeriod::Daily:
        start = end = anchor;
        break;
    case SalesReportPeriod::Weekly:
        start = anchor.addDays(1 - anchor.dayOfWeek());
        end = start.addDays(6);
        break;
    case SalesReportPeriod::Monthly:
        start = QDate(anchor.year(), anchor.month(), 1);
        end = start.addMonths(1).addDays(-1);
        break;
    case SalesReportPeriod::Yearly:
        start = QDate(anchor.year(), 1, 1);
        end = QDate(anchor.year(), 12, 31);
        break;
    case SalesReportPeriod::Custom:
        start = customStart;
        end = customEnd;
        break;
    }
    return validRange(start, end, error);
}

bool cashSalesReport(QDate start, QDate end, SalesReport &report, QString &error)
{
    return generateReport(start, end, true, report, error);
}

bool allSalesReport(QDate start, QDate end, SalesReport &report, QString &error)
{
    return generateReport(start, end, false, report, error);
}
}
