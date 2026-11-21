#pragma once

#include "order_preview.h"

#include <functional>

class QWidget;

namespace UiPages
{
QWidget *createBillPage(const OrderPreview &order, QWidget *parent = nullptr,
    std::function<void()> onNewOrder = {});

bool writeBillPdf(const OrderPreview &order, const QString &path);

// Shows the bill in a pop-up window with Download PDF and Close buttons.
// Used after Generate bill and from Order History.
void showBillDialog(const OrderPreview &order, QWidget *parent);
}
