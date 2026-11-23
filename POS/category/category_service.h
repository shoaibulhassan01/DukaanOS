#pragma once

#include "category.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace Pos
{
// Owner: Umar Khan - the rules for product categories (Increment 3, req 1).
// The Categories page only collects input and calls these functions.
//
// Rules:
//   - Category Id: 2 digits from 10 to 99, never used twice. It is the start
//     of the products' barcodes, and a barcode cannot start with 0, so 00-09
//     are not allowed. It cannot be changed later.
//   - Name and Acronym: required, no '#'. Spaces are saved as '_' so each
//     stays one field in the file. Two categories cannot have the same name.
//   - A category cannot be deleted while a product's barcode starts with
//     its Id (that product would point to a category that no longer exists).

// Trims the text and turns runs of spaces into one '_', e.g. "Soft  drinks" ->
// "Soft_drinks". Used for Name and Acronym before saving.
QString toCategoryField(const QString &text);

// Checks one category against the rules above (not against the other
// categories). Returns false and an error message for the user.
bool validateCategory(const Category &category, QString &error);

// Adds a category to PCategory.txt (creates the file if needed). Refuses an
// invalid category, an Id that is already used and a name that is already used.
bool addCategory(QString categoryFile, Category category, QString &error);

// Changes the Name and Acronym of the category with category.id.
bool updateCategory(QString categoryFile, Category category, QString &error);

// Deletes a category, unless a product in productFile belongs to it.
bool deleteCategory(QString categoryFile, QString productFile, QString categoryId,
    QString &error);

// Where the category comes from:
//   - while a product is being CREATED, from its barcode (first two digits,
//     req 1) - findCategoryForBarcode; addProduct then stores that Id as the
//     11th field of Product.txt (req 2);
//   - for every SAVED product, from that stored 11th field (Product::categoryId)
//     - findCategoryById, productsInCategory, the Products page and the Edit
//     product form.

// The category with this Id in categoryFile. Returns false with a message for
// the user if there is none.
bool findCategoryById(QString categoryFile, QString categoryId, Category &category,
    QString &error);

// For a product being created: the category of its barcode, i.e. of the
// barcode's first two digits (req 1). Returns false with a message for the
// user if the barcode is too short or the category does not exist.
bool findCategoryForBarcode(QString categoryFile, QString barcode, Category &category,
    QString &error);

// Ids of the saved products in productFile whose stored Category Id (11th
// field) is categoryId. A missing product file means none.
bool productsInCategory(QString productFile, QString categoryId, QStringList &productIds,
    QString &error);
}
