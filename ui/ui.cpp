#include "ui.h"

#include "pages/dashboard_page.h"
#include "pages/login_page.h"

#include <QIcon>
#include <QStackedWidget>
#include <QString>

QWidget *createMainWindow()
{
    auto *pages = new QStackedWidget;
    pages->setWindowTitle(QStringLiteral("DukaanOS"));
    pages->setWindowIcon(QIcon(QStringLiteral(":/assets/app_icon.png")));
    pages->setMinimumSize(1080, 700);
    pages->resize(1280, 800);

    auto *login = UiPages::createLoginPage(pages, [pages](const QString &role) {
        auto *dashboard = UiPages::createDashboardPage(pages, role);
        pages->addWidget(dashboard);
        pages->setCurrentWidget(dashboard);
    });
    pages->addWidget(login);
    pages->setCurrentWidget(login);
    return pages;
}
