#pragma once

#include "product.h"

#include <QVector>

namespace Pos
{
// Owner: Umar Khan - find the product that has this barcode.
bool findProductByBarcode(QVector<Product> products, QString barcode,
    Product &foundProduct);

// Owner: Umar Khan - check that the requested quantity is in stock.
bool checkStock(Product product, int requestedQuantity);
}
