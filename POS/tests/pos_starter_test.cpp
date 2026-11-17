#include "../bill/bill_service.h"
#include "../common/record_codec.h"
#include "../product/product_repository.h"
#include "../product/product_service.h"

#include <QCoreApplication>
#include <QFile>
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
    QCoreApplication app(argc, argv);
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
    juice.barcode = QStringLiteral("100000000001");
    juice.acronym = QStringLiteral("APL");
    juice.name = QStringLiteral("Apple_juice");
    juice.unit = QStringLiteral("count");
    juice.unitPrice = 250;
    juice.tax = 12.5;
    juice.discountPerUnit = 10;
    juice.quantity = 48;
    juice.barcodeImage = QStringLiteral("barcodes/100000000001.bmp");
    Pos::Product tea = juice;
    tea.id = QStringLiteral("P0003");
    tea.barcode = QStringLiteral("100000000003");
    tea.acronym = QStringLiteral("TEA");
    tea.name = QStringLiteral("Tea_leaves");
    tea.unit = QStringLiteral("weight");
    tea.quantity = 12;
    tea.barcodeImage = QStringLiteral("barcodes/100000000003.bmp");

    if (!Pos::saveProducts(productFile, { juice, tea }, error))
        return fail(error);
    QFile saved(productFile);
    if (!saved.open(QIODevice::ReadOnly | QIODevice::Text) ||
        QString::fromUtf8(saved.readLine()).trimmed() !=
            QStringLiteral("P0001 100000000001 APL Apple_juice count 250.00 12.50 10.00 48 barcodes/100000000001.bmp#"))
        return fail(QStringLiteral("saveProducts did not write the required record format."));
    saved.close();

    QVector<Pos::Product> loaded;
    if (!Pos::readProducts(productFile, loaded, error))
        return fail(error);
    if (loaded.size() != 2 || loaded.at(1).name != tea.name || loaded.at(0).tax != 12.5)
        return fail(QStringLiteral("Saved products did not read back unchanged."));

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

    Pos::Product pricedProduct;
    pricedProduct.acronym = QStringLiteral("APL");
    pricedProduct.unitPrice = 10;
    pricedProduct.tax = 1.5;
    pricedProduct.discountPerUnit = 1;
    Pos::Order order;
    order.id = QStringLiteral("00000001");
    order.customerName = QStringLiteral("Demo");
    order.status = QStringLiteral("paid");
    Pos::Bill bill;
    if (!Pos::calculateBillLine(QStringLiteral("000000001"), order,
            pricedProduct, 2, bill, error))
        return fail(error);
    if (bill.totalWithoutTax != 20 || bill.totalTax != 3 ||
        bill.totalDiscount != 2 || bill.totalPayable != 21)
        return fail(QStringLiteral("Bill totals do not match the per-unit amounts."));

    QTextStream(stdout) << "PosStarterTest passed" << Qt::endl;
    return 0;
}
