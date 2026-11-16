#pragma once

#include <functional>

class QString;
class QWidget;

namespace UiPages
{
QWidget *createLoginPage(QWidget *parent = nullptr,
    std::function<void(const QString &)> onAuthenticated = {});
}
