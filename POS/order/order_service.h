#pragma once

#include "../bill/bill.h"
#include "cart_item.h"
#include "order.h"

#include <QString>
#include <QVector>

namespace Pos
{
struct OrderSession
{
    QString orderId;
    QString customerName = QStringLiteral("Walk-in");
    QString customerContact;
    QVector<CartItem> items;
};

struct OrderReport
{
    QVector<Order> orders;
    double totalAmount = 0;
};

bool readHeldOrders(QVector<Order> &orders, QString &error);
bool readIncompleteOrdersReport(OrderReport &report, QString &error);
bool readCompletedOrdersReport(OrderReport &report, QString &error);

// A cashier may open an on-hold order directly. Paid orders require manager
// approval and must have been paid within the last 7 days. The caller must only
// pass true after the manager has approved the edit.
bool openOrderForEditing(QString orderId, bool managerApproved,
    OrderSession &session, QString &error,
    bool *managerApprovalRequired = nullptr);

// Saves edits to an on-hold or eligible paid order. Paid-order inventory is
// adjusted by the difference between the saved and edited quantities. For a
// paid order its bill is rebuilt too (same Bill Id, date and time; Bill.txt
// and BillDetail.txt), and the order amount in Order.txt becomes the new
// payable. Money: if the new total is lower, the difference is added to the
// balance returned; if it is higher, the difference is collected, so the
// amount received becomes the new total (balance returned 0).
bool saveOrderEdits(OrderSession &session, bool managerApproved,
    QString &error);

bool addProductToOrder(OrderSession &session, QString barcode, int quantity,
    QString &error);
bool removeProductFromOrder(OrderSession &session, QString productId,
    QString &error);

bool holdOrder(OrderSession &session, QString &error);

bool resumeOrder(QString orderId, OrderSession &session, QString &error);

bool completePayment(OrderSession &session, QString paymentMode,
    QString merchant, double amountPaid, Bill &bill, QString &error);
}
