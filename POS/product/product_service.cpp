#include "product_service.h"

namespace Pos
{
// Owner: Umar Khan - find a product by its exact barcode.
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

// Owner: Umar Khan - check that the requested quantity is in stock.
bool checkStock(Product product, int requestedQuantity)
{
    return false;
}
}
