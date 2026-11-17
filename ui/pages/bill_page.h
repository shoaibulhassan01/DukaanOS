#pragma once

#include "order_preview.h"

#include <functional>

class QWidget;

namespace UiPages
{
QWidget *createBillPage(const OrderPreview &order, QWidget *parent = nullptr,
    std::function<void()> onNewOrder = {});
}
