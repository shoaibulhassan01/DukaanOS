#include "ui.h"

#include "pages/dashboard_page.h"

QWidget *createMainWindow()
{
    return UiPages::createDashboardPage();
}
