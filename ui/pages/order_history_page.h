#pragma once

#include <functional>

class QWidget;

namespace UiPages
{
QWidget *createOrderHistoryPage(QWidget *parent = nullptr,
    std::function<void()> onNewOrder = {});
}
