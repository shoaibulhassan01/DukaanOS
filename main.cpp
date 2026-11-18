#include <memory>

#include <QApplication>
#include <QIcon>
#include <QWidget>

#include "ui/ui.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv); //...
    app.setWindowIcon(QIcon(QStringLiteral(":/assets/app_icon.png")));
    std::unique_ptr<QWidget> window(createMainWindow());
    window->show();
    return app.exec();
}