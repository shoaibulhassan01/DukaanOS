#pragma once

#include "order_preview.h"

#include <functional>

class QWidget;

namespace UiPages
{
QWidget *createOrderHistoryPage(QWidget *parent = nullptr,
    std::function<void()> onNewOrder = {},
    std::function<void(const OrderPreview &, bool)> onEditOrder = {});
}
