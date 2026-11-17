#pragma once

#include "product.h"
#include "../order/cart_item.h"

#include <QString>
#include <QVector>

namespace Pos
{
// Owner: Umar Khan - find the product that has this barcode.
bool findProductByBarcode(QVector<Product> products, QString barcode,
    Product &foundProduct);

// Owner: Umar Khan - check that the requested quantity is in stock.
// Requirement 5: the required quantity must be LESS THAN the available
// quantity, so asking for all of the remaining stock is refused.
bool checkStock(Product product, int requestedQuantity);

// Owner: Umar Khan - after a sale, subtract every sold quantity from
// Product.txt. Products are matched by product id. If any product is missing,
// a quantity is not positive, or stock would go below zero, nothing is saved.
bool reduceStock(QString productFile, QVector<CartItem> soldItems, QString &error);
}
