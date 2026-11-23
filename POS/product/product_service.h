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

// Owner: Umar Khan - check one product before it is saved. Rules:
//   id, name and barcode image path present; no field contains a space or '#';
//   barcode is exactly 6 digits with a first digit of 1-9 (what the
//   Increment 1 encoder and scanner support); acronym is 1-8 characters;
//   unit is "count" or "weight"; price is above 0; tax, discount and
//   quantity are not negative; discount is not more than the price.
bool validateProduct(Product product, QString &error);

// Owner: Umar Khan - add a new product to Product.txt (Manager, Add Product).
// Validates it, checks that the category its barcode starts with exists in
// PCategory.txt (next to Product.txt, req 1), that its id and barcode are not
// already used, stores the Category Id (req 2), creates
// its barcode picture (barcodes/<barcode>.png next to Product.txt, using the
// Increment 1 encoder and generator) and stores "Data/barcodes/<barcode>.png"
// as the 10th field. Product.txt is created if it does not exist yet.
bool addProduct(QString productFile, Product product, QString &error);

// Owner: Umar Khan - edit the price, discount per unit and available
// quantity of an existing product, found by its product id.
bool updateProduct(QString productFile, QString productId, double unitPrice,
    double discountPerUnit, int quantity, QString &error);

// Owner: Umar Khan - products whose id, barcode, acronym or name contains
// the search text (not case sensitive; spaces match the underscores used in
// names). Empty text returns every product.
QVector<Product> searchProducts(QVector<Product> products, QString text);

// Owner: Umar Khan - remove a product from Product.txt (Manager).
// A product that already appears in OrderDetail.txt cannot be deleted,
// because old orders refer to it by id and barcode; set its stock to 0
// instead. A missing orderDetailFile means no orders yet.
bool deleteProduct(QString productFile, QString orderDetailFile, QString productId,
    QString &error);

// Owner: Umar Khan - how a unit is shown on screen. Product.txt stores
// "count" or "weight"; weight is in kilograms.
//   unitLabel("count") -> "count"      unitLabel("weight") -> "weight (kg)"
//   quantityText(36, "count") -> "36 count"   quantityText(100, "weight") -> "100 kg"
QString unitLabel(const QString &unit);
QString quantityText(int quantity, const QString &unit);

// Owner: Umar Khan - products with this quantity or less (low-stock list).
QVector<Product> lowStockProducts(QVector<Product> products, int threshold = 10);
}
