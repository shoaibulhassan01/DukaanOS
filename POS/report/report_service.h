#pragma once

#include "../product/product.h"

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
}
