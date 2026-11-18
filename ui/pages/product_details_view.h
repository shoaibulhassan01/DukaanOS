#pragma once

#include "../../POS/product/product.h"

class QWidget;

namespace UiPages
{
// Owner: Umar Khan - shows all 10 Product.txt fields of one product plus its
// barcode picture. Used by the Scan Product page and by the Description
// button on the New Order page, so both show the same thing.
QWidget *createProductDetailsView(const Pos::Product &product, QWidget *parent = nullptr);

// Opens the details view in a small window (New Order's Description button).
void showProductDescription(const Pos::Product &product, QWidget *parent);
}
