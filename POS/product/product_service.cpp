#include "product_service.h"

#include "product_repository.h"
#include "barcode_image.h"
#include "../category/category_service.h"
#include "../common/record_codec.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>

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

namespace
{
bool hasNoSpaceOrHash(const QString &value)
{
    return !value.contains(QLatin1Char(' ')) && !value.contains(QLatin1Char('#'));
}

bool isSixDigitBarcode(const QString &barcode)
{
    if (barcode.size() != 6 || barcode.at(0) == QLatin1Char('0'))
        return false;
    for (const QChar c : barcode)
    {
        if (!c.isDigit())
            return false;
    }
    return true;
}

int indexOfId(const QVector<Product> &products, const QString &id)
{
    for (int i = 0; i < products.size(); ++i)
    {
        if (products.at(i).id == id)
            return i;
    }
    return -1;
}

// A missing Product.txt simply means there are no products yet.
bool loadForEdit(const QString &productFile, QVector<Product> &products, QString &error)
{
    products.clear();
    if (!QFile::exists(productFile))
        return true;
    return readProducts(productFile, products, error);
}
}

// Owner: Umar Khan - return the first problem found, in field order.
bool validateProduct(Product product, QString &error)
{
    const QString fields[] = { product.id, product.barcode, product.acronym,
        product.name, product.unit, product.barcodeImage };
    for (const QString &field : fields)
    {
        if (!hasNoSpaceOrHash(field))
        {
            error = QStringLiteral("Fields cannot contain spaces or #. Use _ instead of a space.");
            return false;
        }
    }

    if (product.id.isEmpty())
        error = QStringLiteral("Product id is required.");
    else if (!isSixDigitBarcode(product.barcode))
        error = QStringLiteral("Barcode must be exactly 6 digits and cannot start with 0.");
    else if (product.acronym.isEmpty() || product.acronym.size() > 8)
        error = QStringLiteral("Acronym must be 1 to 8 characters.");
    else if (product.name.isEmpty())
        error = QStringLiteral("Product name is required.");
    else if (product.unit != QStringLiteral("count") && product.unit != QStringLiteral("weight"))
        error = QStringLiteral("Unit must be count or weight.");
    else if (product.unitPrice <= 0)
        error = QStringLiteral("Unit price must be greater than 0.");
    else if (product.tax < 0)
        error = QStringLiteral("Tax cannot be negative.");
    else if (product.discountPerUnit < 0)
        error = QStringLiteral("Discount cannot be negative.");
    else if (product.discountPerUnit > product.unitPrice)
        error = QStringLiteral("Discount cannot be more than the unit price.");
    else if (product.quantity < 0)
        error = QStringLiteral("Quantity cannot be negative.");
    else if (product.barcodeImage.isEmpty())
        error = QStringLiteral("Barcode image path is required.");
    else if (!product.categoryId.isEmpty() && product.categoryId != product.barcode.left(2))
        error = QStringLiteral("Category Id %1 must be the first two digits of the barcode (%2).")
            .arg(product.categoryId, product.barcode.left(2));
    else
    {
        error.clear();
        return true;
    }
    return false;
}

QString unitLabel(const QString &unit)
{
    return unit == QStringLiteral("weight") ? QStringLiteral("weight (kg)") : unit;
}

QString quantityText(int quantity, const QString &unit)
{
    return unit == QStringLiteral("weight") ? QStringLiteral("%1 kg").arg(quantity)
                                             : QStringLiteral("%1 %2").arg(quantity).arg(unit).trimmed();
}

// Owner: Umar Khan - validate, check uniqueness, then save.
bool addProduct(QString productFile, Product product, QString &error)
{
    product.barcode = product.barcode.trimmed();
    product.barcodeImage = QStringLiteral("Data/barcodes/%1.png").arg(product.barcode);
    if (!validateProduct(product, error))
        return false;

    // Req 1: the first two digits of the barcode are the Category Id, and the
    // category must be set up first.
    Category category;
    const QString categoryFile = QFileInfo(productFile).absoluteDir().filePath(QStringLiteral("PCategory.txt"));
    if (!findCategoryForBarcode(categoryFile, product.barcode, category, error))
        return false;
    product.categoryId = category.id;   // req 2: stored in Product.txt

    QVector<Product> products;
    if (!loadForEdit(productFile, products, error))
        return false;

    for (const Product &existing : products)
    {
        if (existing.id == product.id)
        {
            error = QStringLiteral("Product id %1 is already used.").arg(product.id);
            return false;
        }
        if (existing.barcode == product.barcode)
        {
            error = QStringLiteral("Barcode %1 is already used by %2.")
                .arg(product.barcode, existing.id);
            return false;
        }
    }

    QString imagePath;
    const QString imageFolder = QFileInfo(productFile).absoluteDir().filePath(QStringLiteral("barcodes"));
    if (!createBarcodeImage(product.barcode, imageFolder, imagePath, error))
        return false;

    products.append(product);
    return saveProducts(productFile, products, error);
}

// Owner: Umar Khan - change only price, discount and quantity.
bool updateProduct(QString productFile, QString productId, double unitPrice,
    double discountPerUnit, int quantity, QString &error)
{
    QVector<Product> products;
    if (!loadForEdit(productFile, products, error))
        return false;

    const int index = indexOfId(products, productId.trimmed());
    if (index < 0)
    {
        error = QStringLiteral("Product %1 is not in Product.txt.").arg(productId);
        return false;
    }

    Product updated = products.at(index);
    updated.unitPrice = unitPrice;
    updated.discountPerUnit = discountPerUnit;
    updated.quantity = quantity;
    if (!validateProduct(updated, error))
        return false;

    products[index] = updated;
    return saveProducts(productFile, products, error);
}

// Owner: Umar Khan - true if productId is on any line of OrderDetail.txt
// (format: orderId productId barcode quantity#).
static bool isProductInOrders(const QString &orderDetailFile, const QString &productId,
    bool &used, QString &error)
{
    used = false;
    if (!QFile::exists(orderDetailFile))
        return true;

    QFile file(orderDetailFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        error = QStringLiteral("Cannot open %1: %2").arg(orderDetailFile, file.errorString());
        return false;
    }
    QTextStream input(&file);
    while (!input.atEnd())
    {
        QStringList fields;
        if (splitRecord(input.readLine(), fields) && fields.size() >= 2 &&
            fields.at(1) == productId)
        {
            used = true;
            return true;
        }
    }
    return true;
}

// Owner: Umar Khan - delete only products that were never sold.
bool deleteProduct(QString productFile, QString orderDetailFile, QString productId,
    QString &error)
{
    productId = productId.trimmed();
    QVector<Product> products;
    if (!loadForEdit(productFile, products, error))
        return false;

    const int index = indexOfId(products, productId);
    if (index < 0)
    {
        error = QStringLiteral("Product %1 is not in Product.txt.").arg(productId);
        return false;
    }

    bool used = false;
    if (!isProductInOrders(orderDetailFile, productId, used, error))
        return false;
    if (used)
    {
        error = QStringLiteral("%1 cannot be deleted because it is part of an existing order. "
                               "Set its stock to 0 instead.").arg(productId);
        return false;
    }

    products.removeAt(index);
    return saveProducts(productFile, products, error);
}

// Owner: Umar Khan - simple contains-search over the identifying fields.
QVector<Product> searchProducts(QVector<Product> products, QString text)
{
    text = text.trimmed();
    if (text.isEmpty())
        return products;
    text.replace(QLatin1Char(' '), QLatin1Char('_'));

    QVector<Product> matches;
    for (const Product &product : products)
    {
        if (product.id.contains(text, Qt::CaseInsensitive) ||
            product.barcode.contains(text, Qt::CaseInsensitive) ||
            product.acronym.contains(text, Qt::CaseInsensitive) ||
            product.name.contains(text, Qt::CaseInsensitive))
            matches.append(product);
    }
    return matches;
}

// Owner: Umar Khan - quantity at or below the threshold counts as low stock.
QVector<Product> lowStockProducts(QVector<Product> products, int threshold)
{
    QVector<Product> lowStock;
    for (const Product &product : products)
    {
        if (product.quantity <= threshold)
            lowStock.append(product);
    }
    return lowStock;
}
}
