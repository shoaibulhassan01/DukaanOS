#pragma once

#include <QString>

class QWidget;

namespace UiPages
{
QWidget *createDashboardPage(QWidget *parent = nullptr,
    const QString &initialRole = QStringLiteral("Cashier"));
}
