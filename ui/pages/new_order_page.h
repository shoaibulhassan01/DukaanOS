#pragma once

#include "order_preview.h"

#include <functional>

class QWidget;

namespace UiPages
{
QWidget *createNewOrderPage(QWidget *parent = nullptr,
    const OrderPreview *initialOrder = nullptr,
    std::function<void(const OrderPreview &)> onGenerateBill = {},
    std::function<void(const OrderPreview &)> onHoldOrder = {},
    std::function<void(const OrderPreview &)> onSaveEdits = {},
    bool editExistingOrder = false,
    bool managerApproved = false);
}
