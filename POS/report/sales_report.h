#pragma once

#include <QDate>
#include <QString>

namespace Pos
{
struct SalesReport
{
    QDate startDate;
    QDate endDate;
    int orderCount = 0;
    double totalSales = 0;
    double totalDiscounts = 0;
};

enum class SalesReportPeriod
{
    Daily,
    Weekly,
    Monthly,
    Yearly,
    Custom
};

bool reportDateRange(SalesReportPeriod period, QDate anchor,
    QDate customStart, QDate customEnd, QDate &start, QDate &end,
    QString &error);
bool cashSalesReport(QDate start, QDate end, SalesReport &report, QString &error);
bool allSalesReport(QDate start, QDate end, SalesReport &report, QString &error);
}
