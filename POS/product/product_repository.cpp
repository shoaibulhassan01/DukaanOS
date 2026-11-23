#include "product_repository.h"

#include "../common/record_codec.h"

#include <QFile>
#include <QSaveFile>
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
        // 11 fields: ... image categoryId (Increment 3). A line from before
        // categories (10 fields) is still read; its category comes from the
        // barcode, and the next save writes the 11th field.
        if (fields.size() != 10 && fields.size() != 11)
        {
            error = QStringLiteral("Product.txt line %1 must contain 11 fields.")
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
        product.categoryId = fields.size() == 11 ? fields.at(10) : product.barcode.left(2);
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

namespace
{
// A text field must be non-empty and must not contain a space or '#',
// otherwise the record could not be read back correctly.
bool isStorableText(const QString &value)
{
    return !value.isEmpty() && !value.contains(QLatin1Char(' ')) &&
        !value.contains(QLatin1Char('#'));
}

QString money(double value)
{
    return QString::number(value, 'f', 2);
}
}

// Owner: Umar Khan - save all products back to Product.txt.
bool saveProducts(QString filePath, QVector<Product> products, QString &error)
{
    QStringList lines;
    for (int i = 0; i < products.size(); ++i)
    {
        const Product &product = products.at(i);
        if (!isStorableText(product.id) || !isStorableText(product.barcode) ||
            !isStorableText(product.acronym) || !isStorableText(product.name) ||
            !isStorableText(product.unit) || !isStorableText(product.barcodeImage) ||
            (!product.categoryId.isEmpty() && !isStorableText(product.categoryId)))
        {
            error = QStringLiteral("Product %1 has an empty field or a field with a space or #.")
                .arg(product.id.isEmpty() ? QString::number(i + 1) : product.id);
            return false;
        }

        lines.append(makeRecord({
            product.id,
            product.barcode,
            product.acronym,
            product.name,
            product.unit,
            money(product.unitPrice),
            money(product.tax),
            money(product.discountPerUnit),
            QString::number(product.quantity),
            product.barcodeImage,
            product.categoryId.isEmpty() ? product.barcode.left(2) : product.categoryId }));
    }

    // QSaveFile writes to a temporary file and only replaces Product.txt
    // when commit() succeeds.
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        error = QStringLiteral("Cannot write %1: %2").arg(filePath, file.errorString());
        return false;
    }

    QTextStream output(&file);
    for (const QString &line : lines)
        output << line << '\n';
    output.flush();

    if (output.status() != QTextStream::Ok || !file.commit())
    {
        error = QStringLiteral("Could not save %1: %2").arg(filePath, file.errorString());
        return false;
    }

    error.clear();
    return true;
}
}
