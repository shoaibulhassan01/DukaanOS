#pragma once

class QString;
class QWidget;

namespace UiPages
{
// Owner: Umar Khan - manager page to set up product categories
// (PCategory.txt, Increment 3 requirement 1).
QWidget *createCategoriesPage(QWidget *parent = nullptr);

// Owner: Umar Khan - the same Add category form, opened from the Add product
// form ("+ Add new category"). Saves through addCategory into categoryFile;
// returns true if a category was added and puts its Id in createdId.
bool showAddCategoryDialog(QWidget *parent, const QString &categoryFile,
    QString *createdId = nullptr);
}
