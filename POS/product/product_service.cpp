#include "product_service.h"

#include "product_repository.h"

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
    return requestedQuantity > 0 && requestedQuantity < product.quantity;
}

// Owner: Umar Khan - read, subtract on a copy, then save once.
bool reduceStock(QString productFile, QVector<CartItem> soldItems, QString &error)
{
    if (soldItems.isEmpty())
    {
        error = QStringLiteral("There are no sold items to take out of stock.");
        return false;
    }

    QVector<Product> products;
    if (!readProducts(productFile, products, error))
        return false;

    for (const CartItem &item : soldItems)
    {
        if (item.quantity <= 0)
        {
            error = QStringLiteral("Sold quantity for %1 must be greater than zero.")
                .arg(item.product.id);
            return false;
        }

        int index = -1;
        for (int i = 0; i < products.size(); ++i)
        {
            if (products.at(i).id == item.product.id)
            {
                index = i;
                break;
            }
        }
        if (index < 0)
        {
            error = QStringLiteral("Product %1 is not in Product.txt.").arg(item.product.id);
            return false;
        }
        if (item.quantity > products.at(index).quantity)
        {
            error = QStringLiteral("Not enough stock of %1: %2 left, %3 sold.")
                .arg(item.product.id)
                .arg(products.at(index).quantity)
                .arg(item.quantity);
            return false;
        }
        products[index].quantity -= item.quantity;
    }

    return saveProducts(productFile, products, error);
}
}
