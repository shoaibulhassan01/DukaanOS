#pragma once

#include "../../POS/product/product.h"

#include <QString>

class QWidget;

namespace UiPages
{
// Owner: Umar Khan - Add Product / Edit Product form for the Products page.
// The form only collects input; the rules and saving are in
// Pos::addProduct and Pos::updateProduct (product_service).

// Shows an empty form. Returns true if a product was added to productFile.
bool showAddProductDialog(QWidget *parent, const QString &productFile);

// Shows the form filled with `product`. Only price, discount and available
// stock can be changed. Returns true if the product was updated.
bool showEditProductDialog(QWidget *parent, const QString &productFile,
    const Pos::Product &product);
}
