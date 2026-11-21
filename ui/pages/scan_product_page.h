#pragma once

class QWidget;

namespace UiPages
{
// Owner: Umar Khan - Scan Product page (Cashier and Manager, requirement 3/5):
// read a barcode with the camera (or type it) and show that product's
// details from Product.txt.
QWidget *createScanProductPage(QWidget *parent = nullptr);
}
