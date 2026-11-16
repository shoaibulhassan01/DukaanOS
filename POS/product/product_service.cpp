#include "product_service.h"

namespace Pos
{
// Find a product by its exact barcode.
bool findProductByBarcode(QVector<Product> products, QString barcode,
    Product &foundProduct)
{
    barcode = barcode.trimmed();
    for (const Product &product : products)
    {
        if (product.barcode == barcode)
        {
            foundProduct = product;
            return true;
        }
    }
    return false;
}

// TODO: Check that the requested quantity is in stock.
bool checkStock(Product product, int requestedQuantity)
{
    return false;
}
}
