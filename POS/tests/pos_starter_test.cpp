#include "../bill/bill_service.h"
#include "../common/record_codec.h"
#include "../product/barcode_image.h"
#include "../product/product_repository.h"
#include "../product/product_service.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QTemporaryDir>
#include <QTextStream>

namespace
{
int fail(const QString &message)
{
    QTextStream(stderr) << message << Qt::endl;
    return 1;
}
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    QString error;
    QStringList fields;

    if (!Pos::splitRecord(
            QStringLiteral("P1 123456 APL Apple count 100 #"), fields))
        return fail(QStringLiteral("Could not split a product record."));
    if (fields.size() != 6 || fields.at(2) != QStringLiteral("APL"))
        return fail(QStringLiteral("RecordCodec parsed unexpected fields."));

    QString encoded;
    encoded = Pos::makeRecord(fields);
    if (encoded != QStringLiteral("P1 123456 APL Apple count 100#"))
        return fail(QStringLiteral("RecordCodec did not encode the record."));
    if (Pos::splitRecord(QStringLiteral("P1 123456"), fields))
        return fail(QStringLiteral("RecordCodec accepted a record without #."));

    // Requirement 5: required quantity must be less than the available quantity.
    Pos::Product product;
    product.quantity = 5;
    if (!Pos::checkStock(product, 4))
        return fail(QStringLiteral("A quantity below the stock must be allowed."));
    if (Pos::checkStock(product, 5))
        return fail(QStringLiteral("A quantity equal to the stock must be refused."));
    if (Pos::checkStock(product, 6))
        return fail(QStringLiteral("A quantity above the stock must be refused."));
    if (Pos::checkStock(product, 0))
        return fail(QStringLiteral("A zero quantity must be refused."));

    // saveProducts + readProducts round trip, and reduceStock, on a temporary copy.
    QTemporaryDir dataDir;
    if (!dataDir.isValid())
        return fail(QStringLiteral("Could not create a temporary folder."));
    const QString productFile = dataDir.filePath(QStringLiteral("Product.txt"));

    Pos::Product juice;
    juice.id = QStringLiteral("P0001");
    juice.barcode = QStringLiteral("100001");
    juice.acronym = QStringLiteral("APL");
    juice.name = QStringLiteral("Apple_juice");
    juice.unit = QStringLiteral("count");
    juice.unitPrice = 250;
    juice.tax = 12.5;
    juice.discountPerUnit = 10;
    juice.quantity = 48;
    juice.barcodeImage = QStringLiteral("barcodes/100001.bmp");
    Pos::Product tea = juice;
    tea.id = QStringLiteral("P0003");
    tea.barcode = QStringLiteral("100003");
    tea.acronym = QStringLiteral("TEA");
    tea.name = QStringLiteral("Tea_leaves");
    tea.unit = QStringLiteral("weight");
    tea.quantity = 12;
    tea.barcodeImage = QStringLiteral("barcodes/100003.bmp");

    if (!Pos::saveProducts(productFile, { juice, tea }, error))
        return fail(error);
    QFile saved(productFile);
    if (!saved.open(QIODevice::ReadOnly | QIODevice::Text) ||
        QString::fromUtf8(saved.readLine()).trimmed() !=
            QStringLiteral("P0001 100001 APL Apple_juice count 250.00 12.50 10.00 48 barcodes/100001.bmp 10#"))
        return fail(QStringLiteral("saveProducts did not write the required record format "
                                   "(11 fields, Category Id last - Increment 3 req 2)."));
    saved.close();

    QVector<Pos::Product> loaded;
    if (!Pos::readProducts(productFile, loaded, error))
        return fail(error);
    if (loaded.size() != 2 || loaded.at(1).name != tea.name || loaded.at(0).tax != 12.5 ||
        loaded.at(0).categoryId != QStringLiteral("10"))
        return fail(QStringLiteral("Saved products did not read back unchanged."));

    // A line from before categories (10 fields) still reads; its category is
    // the first two digits of the barcode. 12 fields is an error.
    const QString oldFormatFile = dataDir.filePath(QStringLiteral("OldProduct.txt"));
    {
        QFile old(oldFormatFile);
        if (!old.open(QIODevice::WriteOnly | QIODevice::Text))
            return fail(QStringLiteral("Could not write a test file."));
        old.write("P0007 340001 CHP Potato_chips count 100.00 5.00 0.00 60 Data/barcodes/340001.png#\n");
    }
    if (!Pos::readProducts(oldFormatFile, loaded, error) || loaded.size() != 1 ||
        loaded.at(0).categoryId != QStringLiteral("34"))
        return fail(QStringLiteral("A 10-field product line must read with Category Id = first two barcode digits."));
    {
        QFile old(oldFormatFile);
        if (!old.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
            return fail(QStringLiteral("Could not write a test file."));
        old.write("P0007 340001 CHP Potato_chips count 100.00 5.00 0.00 60 Data/barcodes/340001.png 34 x#\n");
    }
    if (Pos::readProducts(oldFormatFile, loaded, error))
        return fail(QStringLiteral("A product line with 12 fields must be refused."));

    Pos::Product badProduct = juice;
    badProduct.name = QStringLiteral("Apple juice");
    if (Pos::saveProducts(productFile, { badProduct }, error))
        return fail(QStringLiteral("A name with a space must not be saved."));

    Pos::CartItem soldJuice;
    soldJuice.product = juice;
    soldJuice.quantity = 3;
    Pos::CartItem soldTea;
    soldTea.product = tea;
    soldTea.quantity = 2;
    if (!Pos::reduceStock(productFile, { soldJuice, soldTea }, error))
        return fail(error);
    if (!Pos::readProducts(productFile, loaded, error) ||
        loaded.at(0).quantity != 45 || loaded.at(1).quantity != 10)
        return fail(QStringLiteral("reduceStock did not subtract the sold quantities."));

    Pos::CartItem tooMuchTea = soldTea;
    tooMuchTea.quantity = 11;
    if (Pos::reduceStock(productFile, { soldJuice, tooMuchTea }, error))
        return fail(QStringLiteral("reduceStock accepted more than the stock."));
    if (!Pos::readProducts(productFile, loaded, error) ||
        loaded.at(0).quantity != 45 || loaded.at(1).quantity != 10)
        return fail(QStringLiteral("A failed reduceStock must not change Product.txt."));

    Pos::CartItem unknown = soldJuice;
    unknown.product.id = QStringLiteral("P9999");
    if (Pos::reduceStock(productFile, { unknown }, error))
        return fail(QStringLiteral("reduceStock accepted an unknown product."));

    // validateProduct
    if (!Pos::validateProduct(juice, error))
        return fail(QStringLiteral("A valid product was rejected: ") + error);
    Pos::Product invalid = juice;
    invalid.barcode = QStringLiteral("100000000001");
    if (Pos::validateProduct(invalid, error))
        return fail(QStringLiteral("A 12-digit barcode must be rejected."));
    invalid = juice;
    invalid.barcode = QStringLiteral("012345");
    if (Pos::validateProduct(invalid, error))
        return fail(QStringLiteral("A barcode starting with 0 must be rejected."));
    invalid = juice;
    invalid.acronym = QStringLiteral("TOOLONGXX");
    if (Pos::validateProduct(invalid, error))
        return fail(QStringLiteral("An acronym over 8 characters must be rejected."));
    invalid = juice;
    invalid.unit = QStringLiteral("litre");
    if (Pos::validateProduct(invalid, error))
        return fail(QStringLiteral("A unit other than count/weight must be rejected."));
    invalid = juice;
    invalid.discountPerUnit = 300;
    if (Pos::validateProduct(invalid, error))
        return fail(QStringLiteral("A discount above the price must be rejected."));
    // Product Id: capital P + exactly 4 digits.
    for (const QString &badId : { QStringLiteral("P11"), QStringLiteral("p0001"),
             QStringLiteral("P00001"), QStringLiteral("PA001"), QStringLiteral("0001"),
             QString() })
    {
        invalid = juice;
        invalid.id = badId;
        if (Pos::validateProduct(invalid, error) || !error.contains(QStringLiteral("P followed by 4 digits")))
            return fail(QStringLiteral("Product id '%1' must be rejected.").arg(badId));
    }
    invalid = juice;
    invalid.categoryId = QStringLiteral("20");
    if (Pos::validateProduct(invalid, error))
        return fail(QStringLiteral("A Category Id that is not the start of the barcode must be rejected."));

    // addProduct (file currently holds P0001 x45 and P0003 x10). Req 1: the
    // barcode's first two digits must be a category in PCategory.txt.
    Pos::Product noCategory = juice;
    noCategory.id = QStringLiteral("P0020");
    noCategory.barcode = QStringLiteral("100020");
    if (Pos::addProduct(productFile, noCategory, error) || !error.contains(QStringLiteral("Categories")))
        return fail(QStringLiteral("addProduct must refuse a product while its category does not exist."));
    {
        QFile categories(dataDir.filePath(QStringLiteral("PCategory.txt")));
        if (!categories.open(QIODevice::WriteOnly | QIODevice::Text))
            return fail(QStringLiteral("Could not write a test PCategory.txt."));
        categories.write("10 Beverages BEV#\n");
    }
    noCategory.barcode = QStringLiteral("200020");
    if (Pos::addProduct(productFile, noCategory, error) || !error.contains(QStringLiteral("20")))
        return fail(QStringLiteral("addProduct must refuse barcode 200020: there is no category 20."));
    Pos::Product milk = juice;
    milk.id = QStringLiteral("P0002");
    milk.barcode = QStringLiteral("100002");
    milk.acronym = QStringLiteral("MILK");
    milk.name = QStringLiteral("Dairy_milk");
    milk.quantity = 35;
    milk.barcodeImage = QStringLiteral("barcodes/100002.bmp");
    QString nextId;
    if (!Pos::nextProductId(productFile, nextId, error) || nextId != QStringLiteral("P0004"))
        return fail(QStringLiteral("nextProductId must be one more than the highest id (P0003), got %1.").arg(nextId));
    QString assignedId;
    if (!Pos::addProduct(productFile, milk, error, &assignedId))
        return fail(error);
    if (!Pos::readProducts(productFile, loaded, error) || loaded.last().categoryId != QStringLiteral("10"))
        return fail(QStringLiteral("addProduct must store the Category Id (req 2)."));
    // The id typed in (P0002) is ignored: the next one, P0004, is assigned.
    if (!Pos::readProducts(productFile, loaded, error) || loaded.size() != 3 ||
        loaded.at(2).id != QStringLiteral("P0004") || assignedId != QStringLiteral("P0004"))
        return fail(QStringLiteral("addProduct must add the product with the next id P0004, got %1.").arg(assignedId));
    if (loaded.at(2).barcodeImage != QStringLiteral("Data/barcodes/dairy_milk_100002.png") ||
        !QFile::exists(dataDir.filePath(QStringLiteral("barcodes/dairy_milk_100002.png"))))
        return fail(QStringLiteral("addProduct must create the barcode image and store its path."));
    const QImage savedBarcode(dataDir.filePath(
        QStringLiteral("barcodes/dairy_milk_100002.png")));
    if (savedBarcode.isNull() || savedBarcode.height() <= 200)
        return fail(QStringLiteral("The saved barcode must include the product name and readable barcode."));
    Pos::Product sameBarcode = milk;
    if (Pos::addProduct(productFile, sameBarcode, error))
        return fail(QStringLiteral("addProduct accepted a duplicate barcode."));
    const QString newFile = dataDir.filePath(QStringLiteral("New.txt"));
    if (!Pos::addProduct(newFile, milk, error, &assignedId) || assignedId != QStringLiteral("P0001"))
        return fail(QStringLiteral("addProduct must create a missing Product.txt with id P0001: ") + error);
    {
        const QString fullFile = dataDir.filePath(QStringLiteral("Full.txt"));
        QFile full(fullFile);
        if (!full.open(QIODevice::WriteOnly | QIODevice::Text))
            return fail(QStringLiteral("Could not write a test file."));
        full.write("P9999 100001 CHP Chips count 100.00 5.00 0.00 60 Data/barcodes/100001.png 10#\n");
        full.close();
        if (Pos::nextProductId(fullFile, nextId, error))
            return fail(QStringLiteral("nextProductId must fail once P9999 is used."));
    }

    // updateProduct
    if (!Pos::updateProduct(productFile, QStringLiteral("P0004"), 190, 5, 30, error))
        return fail(error);
    if (!Pos::readProducts(productFile, loaded, error) ||
        loaded.at(2).unitPrice != 190 || loaded.at(2).discountPerUnit != 5 ||
        loaded.at(2).quantity != 30 || loaded.at(2).name != milk.name)
        return fail(QStringLiteral("updateProduct did not change price, discount and stock."));
    if (Pos::updateProduct(productFile, QStringLiteral("P0004"), 190, 5, -1, error))
        return fail(QStringLiteral("updateProduct accepted a negative stock."));
    if (Pos::updateProduct(productFile, QStringLiteral("P9999"), 10, 0, 1, error))
        return fail(QStringLiteral("updateProduct accepted an unknown product."));

    // deleteProduct: an unsold product can go, a sold one cannot
    const QString detailFile = dataDir.filePath(QStringLiteral("OrderDetail.txt"));
    {
        QFile details(detailFile);
        if (!details.open(QIODevice::WriteOnly | QIODevice::Text))
            return fail(QStringLiteral("Could not write a test OrderDetail.txt."));
        details.write("26111701 P0001 100001 2#\n");
    }
    Pos::Product spare = milk;
    spare.barcode = QStringLiteral("100010");
    if (!Pos::addProduct(productFile, spare, error, &assignedId) || assignedId != QStringLiteral("P0005"))
        return fail(QStringLiteral("The next product must get P0005: ") + error);
    const QString spareImage = dataDir.filePath(QStringLiteral("barcodes/") +
        QFileInfo(Pos::barcodeImageFileName(spare.barcode, spare.name)).fileName());
    const QString otherImage = dataDir.filePath(QStringLiteral("barcodes/") +
        Pos::barcodeImageFileName(QStringLiteral("100002"), milk.name));
    if (!QFile::exists(spareImage))
        return fail(QStringLiteral("addProduct must create the barcode picture %1.").arg(spareImage));
    if (!Pos::deleteProduct(productFile, detailFile, QStringLiteral("P0005"), error))
        return fail(QStringLiteral("An unsold product must be deletable: ") + error);
    if (QFile::exists(spareImage))
        return fail(QStringLiteral("Deleting a product must delete its barcode picture."));
    if (!QFile::exists(otherImage))
        return fail(QStringLiteral("Deleting a product must not delete another product's barcode picture."));
    if (Pos::deleteProduct(productFile, detailFile, QStringLiteral("P0001"), error))
        return fail(QStringLiteral("A product that is in an order must not be deleted."));
    if (Pos::deleteProduct(productFile, detailFile, QStringLiteral("P9999"), error))
        return fail(QStringLiteral("deleteProduct accepted an unknown product."));
    if (!Pos::readProducts(productFile, loaded, error) || loaded.size() != 3)
        return fail(QStringLiteral("deleteProduct changed the wrong products."));

    // searchProducts and lowStockProducts (P0001 x45, P0003 x10, P0004 x30)
    if (Pos::searchProducts(loaded, QStringLiteral("dairy milk")).size() != 1 ||
        Pos::searchProducts(loaded, QStringLiteral("10000")).size() != 3 ||
        Pos::searchProducts(loaded, QString()).size() != 3 ||
        !Pos::searchProducts(loaded, QStringLiteral("xyz")).isEmpty())
        return fail(QStringLiteral("searchProducts returned the wrong products."));
    const QVector<Pos::Product> low = Pos::lowStockProducts(loaded);
    if (low.size() != 1 || low.at(0).id != QStringLiteral("P0003"))
        return fail(QStringLiteral("lowStockProducts must list stock of 10 or less."));

    Pos::Product pricedProduct;
    pricedProduct.acronym = QStringLiteral("APL");
    pricedProduct.unitPrice = 10;
    pricedProduct.tax = 1.5;
    pricedProduct.discountPerUnit = 1;
    Pos::BillLine billLine;
    if (!Pos::calculateBillLine(QStringLiteral("000000001"),
            pricedProduct, 2, billLine, error))
        return fail(error);
    if (billLine.totalWithoutTax != 20 || billLine.totalTax != 3 ||
        billLine.detail.totalDiscount != 2 || billLine.totalPayable != 21)
        return fail(QStringLiteral("Bill totals do not match the per-unit amounts."));

    QTextStream(stdout) << "PosStarterTest passed" << Qt::endl;
    return 0;
}
