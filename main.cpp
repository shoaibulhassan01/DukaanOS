#include <memory>

#include <QApplication>
#include <QIcon>
#include <QWidget>

#include "ui/ui.h"
#include "ui/theme.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    UiTheme::install(app);   // light or dark, following Windows
    app.setWindowIcon(QIcon(QStringLiteral(":/assets/app_icon.png")));
    app.setStyleSheet(QStringLiteral(
        "QToolTip { color: white; background-color: #20233B; border: none; padding: 5px 8px; }"));
    std::unique_ptr<QWidget> window(createMainWindow());
    window->show();
    return app.exec();
}