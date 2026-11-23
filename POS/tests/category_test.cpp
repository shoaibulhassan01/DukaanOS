// Owner: Umar Khan - tests for product categories (Increment 3, requirement 1):
// reading and saving PCategory.txt, the add / edit / delete rules, and that
// the saved file passes the data-file format checks. Works in a temporary
// folder; the project's Data/ is never touched.

#include "../category/category_repository.h"
#include "../category/category_service.h"
#include "data_format_checker.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

namespace
{
int failures = 0;

void check(bool condition, const QString &message)
{
    if (!condition)
    {
        QTextStream(stderr) << "FAIL: " << message << Qt::endl;
        ++failures;
    }
}

QString readText(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QStringLiteral("<cannot open>");
    return QString::fromUtf8(file.readAll());
}

void writeText(const QString &path, const QString &text)
{
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(text.toUtf8());
}

Pos::Category category(const QString &id, const QString &name, const QString &acronym)
{
    return Pos::Category{id, name, acronym};
}

int count(const QString &file)
{
    QVector<Pos::Category> categories;
    QString error;
    Pos::readCategories(file, categories, error);
    return categories.size();
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    if (!dir.isValid())
    {
        QTextStream(stderr) << "Could not create a temporary folder." << Qt::endl;
        return 1;
    }
    QDir().mkpath(dir.filePath(QStringLiteral("Data")));
    const QString file = dir.filePath(QStringLiteral("Data/PCategory.txt"));
    const QString products = dir.filePath(QStringLiteral("Data/Product.txt"));
    QString error;
    QVector<Pos::Category> read;

    // ---------------------------------------------------------- empty start
    check(Pos::readCategories(file, read, error) && read.isEmpty(),
        QStringLiteral("A missing PCategory.txt must read as no categories."));

    // ------------------------------------------------------------------ add
    check(Pos::addCategory(file, category(QStringLiteral("10"), QStringLiteral("Beverages"),
              QStringLiteral("BEV")), error),
        QStringLiteral("A valid category was refused: %1").arg(error));
    check(readText(file) == QStringLiteral("10 Beverages BEV#\n"),
        QStringLiteral("PCategory.txt must be exactly \"10 Beverages BEV#\", got: %1").arg(readText(file)));

    check(Pos::addCategory(file, category(QStringLiteral("99"), QStringLiteral("  Soft   drinks "),
              QStringLiteral("SD")), error),
        QStringLiteral("Id 99 and a name with spaces must be accepted: %1").arg(error));
    check(Pos::readCategories(file, read, error) && read.size() == 2 &&
            read.at(1).id == QStringLiteral("99") && read.at(1).name == QStringLiteral("Soft_drinks"),
        QStringLiteral("Spaces in a name must be saved as one '_' (Soft_drinks)."));
    check(readText(file) == QStringLiteral("10 Beverages BEV#\n99 Soft_drinks SD#\n"),
        QStringLiteral("Categories must be saved in the order they were added."));

    // ---------------------------------------------------- refused additions
    struct Refused
    {
        Pos::Category category;
        QString why;
    };
    const QVector<Refused> refused = {
        {category(QStringLiteral("10"), QStringLiteral("Dairy"), QStringLiteral("DRY")), QStringLiteral("a duplicate Id")},
        {category(QStringLiteral("1"), QStringLiteral("Dairy"), QStringLiteral("DRY")), QStringLiteral("a 1-digit Id")},
        {category(QStringLiteral("100"), QStringLiteral("Dairy"), QStringLiteral("DRY")), QStringLiteral("a 3-digit Id")},
        {category(QStringLiteral("AB"), QStringLiteral("Dairy"), QStringLiteral("DRY")), QStringLiteral("letters in the Id")},
        {category(QStringLiteral(""), QStringLiteral("Dairy"), QStringLiteral("DRY")), QStringLiteral("an empty Id")},
        {category(QStringLiteral("1 "), QStringLiteral("Dairy"), QStringLiteral("DRY")), QStringLiteral("a 1-digit Id with a space")},
        {category(QStringLiteral("-1"), QStringLiteral("Dairy"), QStringLiteral("DRY")), QStringLiteral("a negative Id")},
        {category(QStringLiteral("05"), QStringLiteral("Dairy"), QStringLiteral("DRY")), QStringLiteral("Id 05 (barcodes cannot start with 0)")},
        {category(QStringLiteral("00"), QStringLiteral("Dairy"), QStringLiteral("DRY")), QStringLiteral("Id 00")},
        {category(QStringLiteral("09"), QStringLiteral("Dairy"), QStringLiteral("DRY")), QStringLiteral("Id 09")},
        {category(QStringLiteral("20"), QStringLiteral(""), QStringLiteral("DRY")), QStringLiteral("an empty name")},
        {category(QStringLiteral("20"), QStringLiteral("   "), QStringLiteral("DRY")), QStringLiteral("a name of only spaces")},
        {category(QStringLiteral("20"), QStringLiteral("Dairy"), QStringLiteral("")), QStringLiteral("an empty acronym")},
        {category(QStringLiteral("20"), QStringLiteral("Dai#ry"), QStringLiteral("DRY")), QStringLiteral("'#' in the name")},
        {category(QStringLiteral("20"), QStringLiteral("Dairy"), QStringLiteral("D#Y")), QStringLiteral("'#' in the acronym")},
        {category(QStringLiteral("20"), QStringLiteral("beverages"), QStringLiteral("BV2")), QStringLiteral("a name already used (other letter case)")},
    };
    for (const Refused &r : refused)
    {
        const QString before = readText(file);
        check(!Pos::addCategory(file, r.category, error),
            QStringLiteral("addCategory accepted %1.").arg(r.why));
        check(!error.isEmpty(), QStringLiteral("No error message for %1.").arg(r.why));
        check(readText(file) == before, QStringLiteral("A refused category (%1) changed the file.").arg(r.why));
    }

    // ---------------------------------------------------------------- edit
    check(Pos::updateCategory(file, category(QStringLiteral("99"), QStringLiteral("Cold drinks"),
              QStringLiteral("CD")), error),
        QStringLiteral("Editing category 99 failed: %1").arg(error));
    check(readText(file) == QStringLiteral("10 Beverages BEV#\n99 Cold_drinks CD#\n"),
        QStringLiteral("Editing must change only that line, got: %1").arg(readText(file)));
    check(Pos::updateCategory(file, category(QStringLiteral("99"), QStringLiteral("Cold drinks"),
              QStringLiteral("COLD")), error),
        QStringLiteral("Editing a category while keeping its own name must work: %1").arg(error));
    check(!Pos::updateCategory(file, category(QStringLiteral("99"), QStringLiteral("Beverages"),
              QStringLiteral("CD")), error),
        QStringLiteral("Editing must refuse a name used by another category."));
    check(!Pos::updateCategory(file, category(QStringLiteral("77"), QStringLiteral("Bakery"),
              QStringLiteral("BAK")), error),
        QStringLiteral("Editing an unknown category must be refused."));
    check(!Pos::updateCategory(file, category(QStringLiteral("99"), QStringLiteral(""),
              QStringLiteral("CD")), error),
        QStringLiteral("Editing must refuse an empty name."));
    check(count(file) == 2, QStringLiteral("Edits must not add or remove categories."));

    // -------------------------------------------------------------- delete
    // Product P0001 has barcode 100001, so it belongs to category 10.
    writeText(products,
        QStringLiteral("P0001 100001 APL Apple_juice count 250.00 12.50 10.00 36 Data/barcodes/100001.png#\n"));
    QStringList inCategory;
    check(Pos::productsInCategory(products, QStringLiteral("10"), inCategory, error) &&
            inCategory == QStringList({QStringLiteral("P0001")}),
        QStringLiteral("Product P0001 (barcode 100001) must belong to category 10."));
    check(!Pos::deleteCategory(file, products, QStringLiteral("10"), error) &&
            error.contains(QStringLiteral("P0001")),
        QStringLiteral("A category used by a product must not be deleted, and the message must name the product."));
    check(count(file) == 2, QStringLiteral("A refused delete changed the file."));
    check(!Pos::deleteCategory(file, products, QStringLiteral("77"), error),
        QStringLiteral("Deleting an unknown category must be refused."));
    // The category of a barcode is its first two digits.
    Pos::Category found;
    check(Pos::findCategoryForBarcode(file, QStringLiteral("100001"), found, error) &&
            found.id == QStringLiteral("10") && found.name == QStringLiteral("Beverages"),
        QStringLiteral("Barcode 100001 must belong to category 10 (Beverages)."));
    check(!Pos::findCategoryForBarcode(file, QStringLiteral("200001"), found, error) &&
            error.contains(QStringLiteral("20")) && error.contains(QStringLiteral("Categories")),
        QStringLiteral("Barcode 200001 must say category 20 was not found and where to add it, got: %1").arg(error));
    check(!Pos::findCategoryForBarcode(file, QStringLiteral("1"), found, error),
        QStringLiteral("A 1-digit barcode has no category yet."));

    check(Pos::deleteCategory(file, products, QStringLiteral("99"), error),
        QStringLiteral("An unused category could not be deleted: %1").arg(error));
    check(readText(file) == QStringLiteral("10 Beverages BEV#\n"),
        QStringLiteral("Deleting 99 must leave only category 10, got: %1").arg(readText(file)));
    // After a product is saved, its category is the stored 11th field, not
    // the barcode. (The two always match in real data; here the 11th field is
    // set to 88 by hand to prove which one is read.)
    check(Pos::addCategory(file, category(QStringLiteral("88"), QStringLiteral("Test"), QStringLiteral("TST")), error),
        QStringLiteral("Could not add test category 88: %1").arg(error));
    writeText(products,
        QStringLiteral("P0001 100001 APL Apple_juice count 250.00 12.50 10.00 36 Data/barcodes/100001.png 88#\n"));
    check(Pos::productsInCategory(products, QStringLiteral("88"), inCategory, error) &&
            inCategory == QStringList({QStringLiteral("P0001")}),
        QStringLiteral("productsInCategory must use the stored Category Id (11th field)."));
    check(Pos::productsInCategory(products, QStringLiteral("10"), inCategory, error) && inCategory.isEmpty(),
        QStringLiteral("productsInCategory must not use the barcode for a saved product."));
    check(!Pos::deleteCategory(file, products, QStringLiteral("88"), error) && error.contains(QStringLiteral("P0001")),
        QStringLiteral("A category must not be deleted while a product's stored Category Id is that category."));
    Pos::Category byId;
    check(Pos::findCategoryById(file, QStringLiteral("10"), byId, error) && byId.name == QStringLiteral("Beverages"),
        QStringLiteral("findCategoryById must find category 10."));
    check(!Pos::findCategoryById(file, QStringLiteral("77"), byId, error) && error.contains(QStringLiteral("77")),
        QStringLiteral("findCategoryById must report an unknown Id."));
    writeText(products,
        QStringLiteral("P0001 100001 APL Apple_juice count 250.00 12.50 10.00 36 Data/barcodes/100001.png 10#\n"));
    check(Pos::deleteCategory(file, products, QStringLiteral("88"), error),
        QStringLiteral("Test category 88 could not be removed again: %1").arg(error));

    QFile::remove(products);
    check(Pos::productsInCategory(products, QStringLiteral("10"), inCategory, error) && inCategory.isEmpty(),
        QStringLiteral("With no Product.txt, no product belongs to a category."));

    // -------------------------------------------- the saved file's format
    Pos::addCategory(file, category(QStringLiteral("30"), QStringLiteral("Fresh fruit"), QStringLiteral("FRT")), error);
    DataFormat::Report report;
    check(DataFormat::runGroup(QStringLiteral("PCategory"), dir.path(), report) == DataFormat::Result::Passed,
        QStringLiteral("The saved PCategory.txt fails the format checks: %1").arg(report.problems.join(QStringLiteral("; "))));

    // ----------------------------------------------------- broken files
    writeText(file, QStringLiteral("10 Beverages BEV\n"));
    check(!Pos::readCategories(file, read, error), QStringLiteral("A line without '#' must be refused."));
    writeText(file, QStringLiteral("10 Beverages#\n"));
    check(!Pos::readCategories(file, read, error), QStringLiteral("A line with 2 fields must be refused."));
    writeText(file, QStringLiteral("10 Soft drinks SD#\n"));
    check(!Pos::readCategories(file, read, error), QStringLiteral("A line with 4 fields must be refused."));
    check(!Pos::addCategory(file, category(QStringLiteral("20"), QStringLiteral("Dairy"), QStringLiteral("DRY")), error),
        QStringLiteral("Adding to a broken PCategory.txt must be refused, not overwrite it."));
    check(readText(file) == QStringLiteral("10 Soft drinks SD#\n"),
        QStringLiteral("A broken PCategory.txt must be left as it was."));
    writeText(file, QStringLiteral("10 Beverages BEV#\n\n12 Snacks SNK#\r\n"));
    check(Pos::readCategories(file, read, error) && read.size() == 2,
        QStringLiteral("Blank lines and Windows line endings must still read."));

    if (failures > 0)
    {
        QTextStream(stderr) << failures << " check(s) failed." << Qt::endl;
        return 1;
    }
    QTextStream(stdout) << "Category test passed." << Qt::endl;
    return 0;
}
