#include "../bill/bill_service.h"
#include "../common/record_codec.h"
#include "../product/product_service.h"

#include <QCoreApplication>
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

    Pos::Product product;
    product.quantity = 5;
    if (!Pos::checkStock(product, 5))
        return fail(QStringLiteral("Selling the full available stock must be allowed."));
    if (Pos::checkStock(product, 6))
        return fail(QStringLiteral("A request above unreserved stock was accepted."));

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

    return 0;
}
