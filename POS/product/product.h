#pragma once

#include <QString>
#include <QtGlobal>

namespace Pos
{
struct Product
{
    QString id;
    QString barcode;
    QString acronym;
    QString name;
    QString unit;
    double unitPrice = 0;
    double tax = 0;
    double discountPerUnit = 0;
    int quantity = 0;
    QString barcodeImage;
    // Increment 3, req 2: the product's category (PCategory.txt), stored as
    // the 11th field of Product.txt. Always the first two digits of the
    // barcode (req 1).
    QString categoryId;
};
}
