#include "report_service.h"

#include "../order/order_repository.h"
#include "../product/product_repository.h"

#include <QFileInfo>
#include <QHash>

#include <algorithm>

namespace Pos
{
namespace
{
// Name first (case-insensitive), then id: a fixed order for equal numbers.
bool comesFirstByName(const Product &a, const Product &b)
{
    const int byName = a.name.compare(b.name, Qt::CaseInsensitive);
    if (byName != 0)
        return byName < 0;
    return a.id < b.id;
}
}

bool topSoldProducts(QString productFile, QString orderFile, QString orderDetailFile,
    int limit, QVector<SoldItem> &rows, QString &error, QString unitFilter)
{
    rows.clear();
    QVector<Product> products;
    if (!readProducts(productFile, products, error))
        return false;

    QVector<Order> orders;
    if (QFileInfo::exists(orderFile) && !readOrders(orderFile, orders, error))
        return false;

    QHash<QString, int> sold;           // product id -> quantity sold
    QHash<QString, QString> barcodeOf;  // for products no longer in Product.txt
    if (QFileInfo::exists(orderDetailFile))
    {
        for (const Order &order : orders)
        {
            if (order.status != QStringLiteral("paid"))
                continue;
            QVector<OrderDetail> details;
            if (!readOrderDetails(orderDetailFile, order.id, details, error))
                return false;
            for (const OrderDetail &detail : details)
            {
                sold[detail.productId] += detail.quantity;
                barcodeOf.insert(detail.productId, detail.barcode);
            }
        }
    }

    QVector<SoldItem> all;
    for (auto it = sold.cbegin(); it != sold.cend(); ++it)
    {
        if (it.value() <= 0)
            continue;
        SoldItem item;
        item.quantitySold = it.value();
        const auto found = std::find_if(products.cbegin(), products.cend(),
            [&](const Product &p) { return p.id == it.key(); });
        if (found != products.cend())
            item.product = *found;
        else
        {
            item.inProductFile = false;
            item.product.id = it.key();
            item.product.name = it.key();
            item.product.barcode = barcodeOf.value(it.key());
        }
        // A product no longer in Product.txt has no known unit: only in "all".
        if (!unitFilter.isEmpty() && (!item.inProductFile || item.product.unit != unitFilter))
            continue;
        all.append(item);
    }

    std::sort(all.begin(), all.end(), [](const SoldItem &a, const SoldItem &b) {
        if (a.quantitySold != b.quantitySold)
            return a.quantitySold > b.quantitySold;
        return comesFirstByName(a.product, b.product);
    });
    rows = all.mid(0, qMax(0, limit));
    error.clear();
    return true;
}

bool topStockProducts(QString productFile, int limit, QVector<Product> &rows, QString &error,
    QString unitFilter)
{
    rows.clear();
    QVector<Product> products;
    if (!readProducts(productFile, products, error))
        return false;
    if (!unitFilter.isEmpty())
        products.erase(std::remove_if(products.begin(), products.end(),
            [&](const Product &p) { return p.unit != unitFilter; }), products.end());
    std::sort(products.begin(), products.end(), [](const Product &a, const Product &b) {
        if (a.quantity != b.quantity)
            return a.quantity > b.quantity;
        return comesFirstByName(a, b);
    });
    rows = products.mid(0, qMax(0, limit));
    error.clear();
    return true;
}
}
