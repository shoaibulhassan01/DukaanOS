#include "ui.h"

#include "pages/login_page.h"

QWidget *createMainWindow()
{
    return UiPages::createLoginPage();
}
