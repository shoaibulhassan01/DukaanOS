#pragma once

#include <QString>

namespace Pos
{
// Makes the barcode picture for a product by reusing the Increment 1 Code 39
// encoder (encodeText) and generator (generateBarcode). Saves it as
// <folder>/<barcode>.png, enlarged so a camera can read it from a screen or a
// print-out, and returns the saved path in imagePath.
bool createBarcodeImage(const QString &barcode, const QString &folder,
    QString &imagePath, QString &error);
}
