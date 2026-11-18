#include "saved_bill.h"

#include "../../POS/bill/bill_repository.h"
#include "../../POS/common/data_paths.h"
#include "../../POS/order/order_repository.h"
#include "../../POS/product/product_repository.h"

namespace
{
QString readable(QString text)
{
    return text.replace(QLatin1Char('_'), QLatin1Char(' '));
}

QString paymentText(const Pos::Order &order)
{
    QString mode = order.paymentMode;
    if (!mode.isEmpty())
        mode[0] = mode.at(0).toUpper();
    return order.merchant.isEmpty() ? mode
                                    : QStringLiteral("%1 (%2)").arg(mode, readable(order.merchant));
}
}

bool UiPages::loadSavedBill(const Pos::Order &order, OrderPreview &bill, QString &error)
{
    QVector<Pos::Bill> rows;
    if (!Pos::readBillForOrder(Pos::DataPaths::bills(), order.id, rows, error))
        return false;

    // Product names: the order's detail lines are saved in the same order as
    // its bill rows. Names are a nice-to-have, so a missing file is not fatal.
    QVector<Pos::OrderDetail> details;
    QString ignored;
    Pos::readOrderDetails(Pos::DataPaths::orderDetail(), order.id, details, ignored);
    QVector<Pos::Product> products;
    Pos::readProducts(Pos::DataPaths::products(), products, ignored);

    bill = OrderPreview();
    bill.id = rows.first().id;
    bill.orderId = order.id;
    bill.status = order.status;
    bill.customerName = readable(order.customerName);
    bill.phone = order.customerContact;
    bill.createdAt = QStringLiteral("%1  %2").arg(order.date, order.time);
    bill.paymentMethod = paymentText(order);

    for (int i = 0; i < rows.size(); ++i)
    {
        const Pos::Bill &row = rows.at(i);
        OrderPreviewLine line;
        line.quantity = row.quantity;
        line.product.acronym = row.productAcronym;
        line.product.name = row.productAcronym;
        // Prices come from the bill, not from today's Product.txt.
        line.product.unitPrice = row.unitPrice;
        line.product.discountPerUnit = row.discountPerUnit;
        line.product.tax = row.quantity > 0 ? row.totalTax / row.quantity : 0;
        if (i < details.size())
        {
            line.product.id = details.at(i).productId;
            line.product.barcode = details.at(i).barcode;
            for (const Pos::Product &product : products)
            {
                if (product.id == line.product.id)
                    line.product.name = product.name;
            }
        }
        bill.lines.append(line);
        bill.itemCount += row.quantity;
        bill.subtotal += row.totalWithoutTax;
        bill.tax += row.totalTax;
        bill.discount += row.totalDiscount;
        bill.total += row.totalPayable;
    }
    error.clear();
    return true;
}
