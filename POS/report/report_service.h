#pragma once

#include "../bill/bill.h"
#include "../product/product.h"

#include <QDate>
#include <QString>
#include <QVector>

namespace Pos
{
// Owner: Umar Khan - manager reports from Increment 3, requirement 11:
//   - the top ten most sold items (in descending order) with their remaining
//     stocks;
//   - the top ten items with the most remaining stock (in descending order),
//     so that purchase of these is not initiated.
// Both only read the data files; nothing is written.
//
// unitFilter: "" for all items, or "count" / "weight" to rank only items
// measured that way (counts and kilograms cannot be compared with each
// other). The filter is applied before taking the top ten.

struct SoldItem
{
    Product product;        // from Product.txt (remaining stock = product.quantity)
    int quantitySold = 0;   // total over all paid orders
    bool inProductFile = true;   // false if the product is no longer in Product.txt
};

// Items sold in PAID orders (Order.txt status "paid"; on-hold and archived
// orders are not sales), totalled per product from OrderDetail.txt, most sold
// first. Ties are ordered by product name, then id, so the result is always
// the same. At most `limit` rows; fewer if fewer products were ever sold.
// A missing Order.txt or OrderDetail.txt means nothing has been sold yet.
bool topSoldProducts(QString productFile, QString orderFile, QString orderDetailFile,
    int limit, QVector<SoldItem> &rows, QString &error, QString unitFilter = QString());

// Products with the most remaining stock (Product.txt quantity), highest
// first. Ties by product name, then id. At most `limit` rows.
bool topStockProducts(QString productFile, int limit, QVector<Product> &rows, QString &error,
    QString unitFilter = QString());

// ---------------------------------------------------------------------------
// Sales reports (req 11): "list all the cash sales (daily, weekly, monthly,
// yearly, specified time period)" and "all sales with total amount and
// discounts". Agreed rules:
//   - a sale is a PAID order (Order.txt status "paid") with its bill in
//     Bill.txt; on-hold, in-progress and archived (discarded) orders are not
//     sales;
//   - a sale's date is its bill date (when it was paid), and its amount is
//     the bill's total payable;
//   - periods are calendar periods around a chosen date: the day, its week
//     (Monday to Sunday), its month, or its year.
// ---------------------------------------------------------------------------
enum class SalesPeriod { Daily, Weekly, Monthly, Yearly };

// The first and last day of the calendar period that contains `date`.
void salesPeriodRange(SalesPeriod period, QDate date, QDate &from, QDate &to);

struct Sale
{
    Bill bill;              // header from Bill.txt (no details)
    QString paymentMode;    // cash, card or wallet (from Order.txt)
};

struct SalesReport
{
    QVector<Sale> sales;              // oldest first (bill date, time, id)
    double totalWithoutTax = 0;
    double totalTax = 0;
    double productDiscounts = 0;      // Bill.txt "total applicable discount"
    double seasonalDiscounts = 0;
    double specialDiscounts = 0;
    double totalDiscounts = 0;        // the three discounts together
    double totalAmount = 0;           // sum of total payable
};

// Cash sales whose bill date is from `from` to `to` (both included).
bool cashSalesReport(QString orderFile, QString billFile, QDate from, QDate to,
    SalesReport &report, QString &error);

// Every sale (any payment mode), with totals of amounts and discounts.
bool allSalesReport(QString orderFile, QString billFile, SalesReport &report, QString &error);
}
