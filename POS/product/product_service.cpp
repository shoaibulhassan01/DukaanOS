#include "product_service.h"

namespace Pos
{
// TODO: Find the product that has this barcode.
bool findProductByBarcode(QVector<Product> products, QString barcode,
    Product &foundProduct)
{
    return false;
}

// TODO: Check that the requested quantity is in stock.
bool checkStock(Product product, int requestedQuantity)
{
    return false;
}
}
