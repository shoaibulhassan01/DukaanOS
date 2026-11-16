#pragma once

#include "order_preview.h"

#include <functional>
#include <QVector>

class QWidget;

namespace UiPages
{
QWidget *createHeldOrdersPage(const QVector<OrderPreview> &orders, QWidget *parent = nullptr,
    std::function<void(int)> onResume = {}, std::function<void(int)> onDiscard = {});
}
