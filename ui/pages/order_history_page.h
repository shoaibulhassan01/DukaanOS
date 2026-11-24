#pragma once

#include "order_preview.h"

#include <functional>

class QWidget;

namespace UiPages
{
// signedInAsManager: the user who logged in is a manager, so editing a
// paid order needs no separate approval (req 3: approval is for cashiers).
QWidget *createOrderHistoryPage(QWidget *parent = nullptr,
    std::function<void()> onNewOrder = {},
    std::function<void(const OrderPreview &, bool)> onEditOrder = {},
    bool signedInAsManager = false);
}
