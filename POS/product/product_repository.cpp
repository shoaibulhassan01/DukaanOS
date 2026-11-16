#include "product_repository.h"

#include <QFile>
#include <QTextStream>

namespace Pos
{
// Read the product records from the selected text file.
bool readProducts(QString filePath, QVector<Product> &products, QString &error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        error = QStringLiteral("Cannot open %1: %2").arg(filePath, file.errorString());
        return false;
    }

    QVector<Product> loadedProducts;
    QTextStream input(&file);
    int lineNumber = 0;
    while (!input.atEnd())
    {
        const QString line = input.readLine().trimmed();
        ++lineNumber;
        if (line.isEmpty())
            continue;
        if (!line.endsWith(QLatin1Char('#')))
        {
            error = QStringLiteral("Product.txt line %1 is missing the ending #.")
                .arg(lineNumber);
            return false;
        }

        const QStringList fields = line.left(line.size() - 1)
            .split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (fields.size() != 10)
        {
            error = QStringLiteral("Product.txt line %1 must contain 10 fields.")
                .arg(lineNumber);
            return false;
        }

        bool validPrice = false;
        bool validTax = false;
        bool validDiscount = false;
        bool validQuantity = false;
        Product product;
        product.id = fields.at(0);
        product.barcode = fields.at(1);
        product.acronym = fields.at(2);
        product.name = fields.at(3);
        product.unit = fields.at(4);
        product.unitPrice = fields.at(5).toDouble(&validPrice);
        product.tax = fields.at(6).toDouble(&validTax);
        product.discountPerUnit = fields.at(7).toDouble(&validDiscount);
        product.quantity = fields.at(8).toInt(&validQuantity);
        product.barcodeImage = fields.at(9);
        if (!validPrice || !validTax || !validDiscount || !validQuantity)
        {
            error = QStringLiteral("Product.txt line %1 has an invalid price, tax, discount or quantity.")
                .arg(lineNumber);
            return false;
        }
        loadedProducts.append(product);
    }

    products = loadedProducts;
    error.clear();
    return true;
}

// TODO: Save all products back to Product.txt.
bool saveProducts(QString filePath, QVector<Product> products, QString &error)
{
    error = QStringLiteral("TODO: Save products to Product.txt.");
    return false;
}
}
