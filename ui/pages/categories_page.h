#pragma once

class QString;
class QWidget;

namespace UiPages
{
// Owner: Umar Khan - manager page to set up product categories
// (PCategory.txt, Increment 3 requirement 1).
QWidget *createCategoriesPage(QWidget *parent = nullptr);

// Owner: Umar Khan - the same Add category form, opened from the Add product
// form when the barcode's category does not exist yet. The Id (the barcode's
// first two digits) is filled in and locked. Saves through addCategory into
// categoryFile; returns true if the category was added.
bool showAddCategoryForBarcode(QWidget *parent, const QString &categoryFile,
    const QString &categoryId);
}
