#pragma once

#include <QImage>
#include <QString>

namespace Pos
{
// Creates a printable barcode image containing the product name and barcode.
QImage renderBarcodeImage(const QString &barcode, const QString &productName,
    QString &error);

// Name-based and barcode-qualified so filenames remain readable and unique.
QString barcodeImageFileName(const QString &barcode, const QString &productName);

bool createBarcodeImage(const QString &barcode, const QString &productName,
    const QString &folder,
    QString &imagePath, QString &error);
}
