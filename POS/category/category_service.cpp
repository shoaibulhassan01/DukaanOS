#include "category_service.h"

#include "category_repository.h"
#include "../product/product_repository.h"

#include <QFileInfo>
#include <QRegularExpression>

namespace Pos
{
QString toCategoryField(const QString &text)
{
    return text.simplified().replace(QLatin1Char(' '), QLatin1Char('_'));
}

bool validateCategory(const Category &category, QString &error)
{
    // 2 digits, 10-99: the Id starts every barcode in the category, and a
    // barcode cannot start with 0.
    static const QRegularExpression tenToNinetyNine(QStringLiteral("^[1-9]\\d$"));
    if (!tenToNinetyNine.match(category.id).hasMatch())
    {
        error = QStringLiteral("Category Id must be 2 digits from 10 to 99 (barcodes cannot start with 0).");
        return false;
    }
    if (category.name.isEmpty())
    {
        error = QStringLiteral("Enter a category name.");
        return false;
    }
    if (category.acronym.isEmpty())
    {
        error = QStringLiteral("Enter a category acronym.");
        return false;
    }
    for (const QString &field : {category.name, category.acronym})
    {
        if (field.contains(QLatin1Char('#')) || field.contains(QLatin1Char(' ')) ||
            field.contains(QLatin1Char('\t')))
        {
            error = QStringLiteral("Name and acronym cannot contain '#' or spaces.");
            return false;
        }
    }
    error.clear();
    return true;
}

namespace
{
Category cleaned(Category category)
{
    category.id = category.id.trimmed();
    category.name = toCategoryField(category.name);
    category.acronym = toCategoryField(category.acronym);
    return category;
}

bool sameName(const Category &a, const Category &b)
{
    return a.name.compare(b.name, Qt::CaseInsensitive) == 0;
}
}

bool addCategory(QString categoryFile, Category category, QString &error)
{
    category = cleaned(category);
    if (!validateCategory(category, error))
        return false;

    QVector<Category> categories;
    if (!readCategories(categoryFile, categories, error))
        return false;
    for (const Category &existing : categories)
    {
        if (existing.id == category.id)
        {
            error = QStringLiteral("Category Id %1 is already used by %2.")
                .arg(existing.id, QString(existing.name).replace(QLatin1Char('_'), QLatin1Char(' ')));
            return false;
        }
        if (sameName(existing, category))
        {
            error = QStringLiteral("A category named %1 already exists (Id %2).")
                .arg(QString(existing.name).replace(QLatin1Char('_'), QLatin1Char(' ')), existing.id);
            return false;
        }
    }
    categories.append(category);
    return saveCategories(categoryFile, categories, error);
}

bool updateCategory(QString categoryFile, Category category, QString &error)
{
    category = cleaned(category);
    if (!validateCategory(category, error))
        return false;

    QVector<Category> categories;
    if (!readCategories(categoryFile, categories, error))
        return false;
    int index = -1;
    for (int i = 0; i < categories.size(); ++i)
    {
        if (categories.at(i).id == category.id)
            index = i;
        else if (sameName(categories.at(i), category))
        {
            error = QStringLiteral("A category named %1 already exists (Id %2).")
                .arg(QString(category.name).replace(QLatin1Char('_'), QLatin1Char(' ')), categories.at(i).id);
            return false;
        }
    }
    if (index < 0)
    {
        error = QStringLiteral("Category %1 was not found in PCategory.txt.").arg(category.id);
        return false;
    }
    categories[index] = category;
    return saveCategories(categoryFile, categories, error);
}

bool findCategoryForBarcode(QString categoryFile, QString barcode, Category &category,
    QString &error)
{
    barcode = barcode.trimmed();
    if (barcode.size() < 2)
    {
        error = QStringLiteral("Enter the barcode: its first two digits are the Category Id.");
        return false;
    }
    if (findCategoryById(categoryFile, barcode.left(2), category, error))
        return true;
    if (error.startsWith(QStringLiteral("No category")))
        error = QStringLiteral("No category with Id %1 (the first two digits of the barcode). "
                               "Add it on the Categories page first.").arg(barcode.left(2));
    return false;
}

bool findCategoryById(QString categoryFile, QString categoryId, Category &category,
    QString &error)
{
    QVector<Category> categories;
    if (!readCategories(categoryFile, categories, error))
        return false;
    for (const Category &existing : categories)
    {
        if (existing.id == categoryId)
        {
            category = existing;
            error.clear();
            return true;
        }
    }
    error = QStringLiteral("No category with Id %1 in PCategory.txt.").arg(categoryId);
    return false;
}

QVector<Category> searchCategories(const QVector<Category> &categories, QString text)
{
    text = text.trimmed();
    if (text.isEmpty())
        return categories;
    text.replace(QLatin1Char(' '), QLatin1Char('_'));

    QVector<Category> matches;
    for (const Category &category : categories)
    {
        if (category.id.contains(text, Qt::CaseInsensitive) ||
            category.name.contains(text, Qt::CaseInsensitive) ||
            category.acronym.contains(text, Qt::CaseInsensitive))
            matches.append(category);
    }
    return matches;
}

bool productsInCategory(QString productFile, QString categoryId, QStringList &productIds,
    QString &error)
{
    productIds.clear();
    if (!QFileInfo::exists(productFile))
    {
        error.clear();
        return true;
    }
    QVector<Product> products;
    if (!readProducts(productFile, products, error))
        return false;
    for (const Product &product : products)
        if (product.categoryId == categoryId)   // the stored 11th field
            productIds.append(product.id);
    error.clear();
    return true;
}

bool deleteCategory(QString categoryFile, QString productFile, QString categoryId,
    QString &error)
{
    QVector<Category> categories;
    if (!readCategories(categoryFile, categories, error))
        return false;
    int index = -1;
    for (int i = 0; i < categories.size(); ++i)
        if (categories.at(i).id == categoryId)
            index = i;
    if (index < 0)
    {
        error = QStringLiteral("Category %1 was not found in PCategory.txt.").arg(categoryId);
        return false;
    }

    QStringList used;
    if (!productsInCategory(productFile, categoryId, used, error))
        return false;
    if (!used.isEmpty())
    {
        error = QStringLiteral("Category %1 cannot be deleted: %2 product(s) belong to it (%3). "
                               "Move or delete those products first.")
            .arg(categoryId).arg(used.size()).arg(used.join(QStringLiteral(", ")));
        return false;
    }
    categories.removeAt(index);
    return saveCategories(categoryFile, categories, error);
}
}
