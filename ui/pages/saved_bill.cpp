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
    Pos::Bill saved;
    if (!Pos::readBillForOrder(Pos::DataPaths::bills(),
            Pos::DataPaths::billDetails(), order.id, saved, error))
        return false;

    // Product names: the order's detail lines are saved in the same order as
    // its bill rows. Names are a nice-to-have, so a missing file is not fatal.
    QVector<Pos::OrderDetail> details;
    QString ignored;
    Pos::readOrderDetails(Pos::DataPaths::orderDetail(), order.id, details, ignored);
    QVector<Pos::Product> products;
    Pos::readProducts(Pos::DataPaths::products(), products, ignored);

    bill = OrderPreview();
    bill.id = saved.id;
    bill.orderId = order.id;
    bill.status = order.status;
    bill.customerName = readable(order.customerName);
    bill.phone = order.customerContact;
    bill.createdAt = QStringLiteral("%1  %2").arg(saved.date, saved.time);
    bill.paymentMethod = paymentText(order);

    for (int i = 0; i < saved.details.size(); ++i)
    {
        const Pos::BillDetail &detail = saved.details.at(i);
        OrderPreviewLine line;
        line.quantity = detail.quantity;
        line.product.acronym = detail.productAcronym;
        line.product.name = detail.productAcronym;
        line.product.unitPrice = detail.unitPrice;
        line.product.discountPerUnit = detail.quantity > 0
            ? detail.totalDiscount / detail.quantity : 0;
        if (i < details.size())
        {
            const Pos::OrderDetail &orderDetail = details.at(i);
            line.product.id = orderDetail.productId;
            line.product.barcode = orderDetail.barcode;
            for (const Pos::Product &product : products)
            {
                if (product.id == orderDetail.productId
                    && product.barcode == orderDetail.barcode)
                {
                    line.product.name = product.name;
                    break;
                }
            }
        }
        bill.lines.append(line);
        bill.itemCount += detail.quantity;
    }
    bill.subtotal = saved.totalWithoutTax;
    bill.tax = saved.totalTax;
    bill.discount = saved.totalDiscount;
    bill.seasonalDiscount = saved.seasonalDiscount;
    bill.specialDiscount = saved.specialDiscount;
    bill.total = saved.totalPayable;
    error.clear();
    return true;
}
