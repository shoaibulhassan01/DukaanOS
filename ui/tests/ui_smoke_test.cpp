// Owner: Umar Khan - opens the real screens without a display (offscreen) and
// clicks through them, so a page that crashes or a broken role lock is caught
// in the pull request. Works on a temporary copy of Data/, so the project's
// sample data is never changed.
//
// Covers: login checks (empty fields, wrong security answer); the pages each
// role may open (Manager / Cashier / unknown role); every page in light and
// dark mode; a cashier sale: add item, hold, resume, pay in cash, bill saved,
// stock reduced and the order shown in Order History.

#include "../ui.h"
#include "../theme.h"
#include "../pages/dashboard_page.h"

#include "../../POS/order/order_repository.h"
#include "../../POS/product/product_repository.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDirIterator>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

#ifndef DUKAANOS_SOURCE_DIR
#error "DUKAANOS_SOURCE_DIR must point at the project folder (set in CMakeLists.txt)."
#endif

namespace
{
int failures = 0;

void check(bool condition, const QString &message)
{
    if (!condition)
    {
        QTextStream(stderr) << "FAIL: " << message << Qt::endl;
        ++failures;
    }
}

// Lets Qt finish pending work (layouts, deleteLater) after a click.
void settle()
{
    for (int i = 0; i < 5; ++i)
    {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
    }
}

bool copyFolder(const QString &from, const QString &to)
{
    QDir().mkpath(to);
    QDirIterator it(from, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        const QString source = it.next();
        const QString target = QDir(to).filePath(QDir(from).relativeFilePath(source));
        if (it.fileInfo().isDir())
            QDir().mkpath(target);
        else if (!QFile::copy(source, target))
            return false;
    }
    return true;
}

QPushButton *button(QWidget *root, const QString &text)
{
    for (QPushButton *b : root->findChildren<QPushButton *>())
        if (b->isVisible() && b->text().trimmed() == text)
            return b;
    return nullptr;
}

QLineEdit *field(QWidget *root, const QString &placeholder)
{
    for (QLineEdit *e : root->findChildren<QLineEdit *>())
        if (e->isVisible() && e->placeholderText().startsWith(placeholder))
            return e;
    return nullptr;
}

QPushButton *themeButton(QWidget *root)
{
    for (QPushButton *b : root->findChildren<QPushButton *>())
        if (b->isVisible() && b->toolTip().startsWith(QStringLiteral("Switch to")))
            return b;
    return nullptr;
}

bool hasLabel(QWidget *root, const QString &text)
{
    for (QLabel *label : root->findChildren<QLabel *>())
        if (label->isVisible() && label->text() == text)
            return true;
    return false;
}

// True if any visible label or table cell contains the text.
bool showsText(QWidget *root, const QString &text)
{
    for (QLabel *label : root->findChildren<QLabel *>())
        if (label->isVisible() && label->text().contains(text))
            return true;
    for (QAbstractItemView *view : root->findChildren<QAbstractItemView *>())
    {
        QAbstractItemModel *model = view->model();
        if (!view->isVisible() || !model)
            continue;
        for (int r = 0; r < model->rowCount(); ++r)
            for (int c = 0; c < model->columnCount(); ++c)
                if (model->data(model->index(r, c)).toString().contains(text))
                    return true;
    }
    return false;
}

// The sidebar buttons ("   Dashboard", "   Products", ...).
QStringList sidebarPages(QWidget *dashboard)
{
    QStringList pages;
    for (QPushButton *b : dashboard->findChildren<QPushButton *>())
        if (b->isVisible() && b->text().startsWith(QStringLiteral("   ")))
            pages.append(b->text().trimmed());
    return pages;
}

QWidget *openDashboard(QStackedWidget *stack, const QString &role)
{
    QWidget *dashboard = UiPages::createDashboardPage(stack, role);
    stack->addWidget(dashboard);
    stack->setCurrentWidget(dashboard);
    settle();
    return dashboard;
}

// Clicks every sidebar page and checks the header shows its name.
void visitAllPages(QWidget *dashboard, const QString &who)
{
    for (const QString &page : sidebarPages(dashboard))
    {
        QPushButton *b = nullptr;
        for (QPushButton *candidate : dashboard->findChildren<QPushButton *>())
            if (candidate->isVisible() && candidate->text() == QStringLiteral("   ") + page)
                b = candidate;
        if (!b)
            continue;
        b->click();
        settle();
        check(hasLabel(dashboard, page),
            QStringLiteral("%1: the %2 page did not open.").arg(who, page));
    }
}

// Clicks Close on any pop-up (bill dialog) that opens, so exec() returns.
void closePopupsAutomatically()
{
    auto *timer = new QTimer(qApp);
    QObject::connect(timer, &QTimer::timeout, []() {
        QWidget *popup = QApplication::activeModalWidget();
        if (!popup)
            return;
        for (QPushButton *b : popup->findChildren<QPushButton *>())
            if (b->text().trimmed() == QStringLiteral("Close") || b->text().trimmed() == QStringLiteral("OK"))
            {
                b->click();
                return;
            }
        popup->close();
    });
    timer->start(50);
}

int stockOf(const QString &productId)
{
    QVector<Pos::Product> products;
    QString error;
    Pos::readProducts(QStringLiteral("Data/Product.txt"), products, error);
    for (const Pos::Product &p : products)
        if (p.id == productId)
            return p.quantity;
    return -1;
}

void testLogin(QWidget *window)
{
    auto *stack = qobject_cast<QStackedWidget *>(window);
    QPushButton *login = button(window, QStringLiteral("Login"));
    check(login, QStringLiteral("Login page: no Login button."));
    if (!login)
        return;

    login->click();
    settle();
    check(stack->count() == 1 && hasLabel(window, QStringLiteral("Enter your username and password.")),
        QStringLiteral("Login page: empty fields must be refused with a message."));

    QLineEdit *user = field(window, QStringLiteral("Username"));
    QLineEdit *pass = field(window, QStringLiteral("Password"));
    QLineEdit *answer = field(window, QStringLiteral("Type the answer"));
    check(user && pass && answer, QStringLiteral("Login page: username, password or answer box missing."));
    if (!user || !pass || !answer)
        return;

    user->setText(QStringLiteral("manager"));
    pass->setText(QStringLiteral("manager123"));
    answer->setText(QStringLiteral("abc"));
    login->click();
    settle();
    check(stack->count() == 1 && hasLabel(window, QStringLiteral("Solve the security question to continue.")),
        QStringLiteral("Login page: a non-number answer must be refused."));

    // The question is two numbers from 2 to 9 added together, so 0 is always wrong.
    pass->setText(QStringLiteral("manager123"));
    answer->setText(QStringLiteral("0"));
    login->click();
    settle();
    check(stack->count() == 1 && hasLabel(window, QStringLiteral("Wrong answer. Please try the new question.")),
        QStringLiteral("Login page: a wrong security answer must not log in."));

    QPushButton *theme = themeButton(window);
    check(theme, QStringLiteral("Login page: no sun/moon button."));
    if (theme)
    {
        theme->click();
        settle();
        check(UiTheme::isDark(), QStringLiteral("Login page: the moon button did not switch to dark mode."));
        theme->click();
        settle();
        check(!UiTheme::isDark(), QStringLiteral("Login page: the sun button did not switch back to light mode."));
    }
}

void testRoles(QStackedWidget *stack)
{
    QWidget *manager = openDashboard(stack, QStringLiteral("Manager"));
    check(sidebarPages(manager) == QStringList({QStringLiteral("Dashboard"), QStringLiteral("Scan Product"),
                                                QStringLiteral("Products"), QStringLiteral("Order History")}),
        QStringLiteral("Manager sees the wrong pages: %1").arg(sidebarPages(manager).join(", ")));
    for (QComboBox *box : manager->findChildren<QComboBox *>())
        if (box->findText(QStringLiteral("Manager")) >= 0 && box->findText(QStringLiteral("Cashier")) >= 0)
            check(!box->isEnabled(), QStringLiteral("The role box must be locked after login."));
    visitAllPages(manager, QStringLiteral("Manager (light)"));

    QPushButton *theme = themeButton(manager);
    check(theme, QStringLiteral("Dashboard: no sun/moon button in the header."));
    if (theme)
    {
        theme->click();
        settle();
        check(UiTheme::isDark(), QStringLiteral("Dashboard: the moon button did not switch to dark mode."));
        visitAllPages(manager, QStringLiteral("Manager (dark)"));
        if (QPushButton *again = themeButton(manager))
            again->click();
        settle();
        check(!UiTheme::isDark(), QStringLiteral("Dashboard: could not switch back to light mode."));
    }

    QWidget *cashier = openDashboard(stack, QStringLiteral("Cashier"));
    const QStringList cashierPages = {QStringLiteral("Dashboard"), QStringLiteral("New Order"),
        QStringLiteral("Scan Product"), QStringLiteral("Held Orders"), QStringLiteral("Order History")};
    check(sidebarPages(cashier) == cashierPages,
        QStringLiteral("Cashier sees the wrong pages: %1").arg(sidebarPages(cashier).join(", ")));
    check(!sidebarPages(cashier).contains(QStringLiteral("Products")),
        QStringLiteral("A cashier must not be able to open Products."));
    visitAllPages(cashier, QStringLiteral("Cashier"));

    QWidget *unknown = openDashboard(stack, QStringLiteral("admin"));
    check(sidebarPages(unknown) == cashierPages,
        QStringLiteral("An unknown role must get the least access (Cashier)."));
}

void testSale(QStackedWidget *stack)
{
    QVector<Pos::Order> before;
    QString error;
    Pos::readOrders(QStringLiteral("Data/Order.txt"), before, error);
    const int stockBefore = stockOf(QStringLiteral("P0001"));

    QWidget *cashier = openDashboard(stack, QStringLiteral("Cashier"));
    QPushButton *newOrder = button(cashier, QStringLiteral("New Order"));
    check(newOrder, QStringLiteral("Cashier: no New Order button."));
    if (!newOrder)
        return;
    newOrder->click();
    settle();

    QLineEdit *customer = field(cashier, QStringLiteral("Enter customer name"));
    QLineEdit *barcode = field(cashier, QStringLiteral("Enter product barcode"));
    QPushButton *add = button(cashier, QStringLiteral("Add item"));
    check(customer && barcode && add, QStringLiteral("New Order: customer, barcode or Add item missing."));
    if (!customer || !barcode || !add)
        return;
    customer->setText(QStringLiteral("CI Test"));
    barcode->setText(QStringLiteral("100001"));
    add->click();
    settle();

    // Hold the order, then resume it from Held Orders.
    QPushButton *hold = button(cashier, QStringLiteral("Hold order"));
    check(hold, QStringLiteral("New Order: no Hold order button."));
    if (!hold)
        return;
    hold->click();
    settle();
    check(hasLabel(cashier, QStringLiteral("Held Orders")) && showsText(cashier, QStringLiteral("HOLD-0001")),
        QStringLiteral("Holding an order must open Held Orders and list HOLD-0001."));
    QPushButton *resume = button(cashier, QStringLiteral("Resume"));
    check(resume, QStringLiteral("Held Orders: no Resume button."));
    if (!resume)
        return;
    resume->click();
    settle();
    check(hasLabel(cashier, QStringLiteral("New Order")),
        QStringLiteral("Resume must go back to New Order."));

    // Pay in cash. The bill pop-up is closed automatically.
    for (QComboBox *box : cashier->findChildren<QComboBox *>())
        if (box->isVisible() && box->findText(QStringLiteral("Cash")) >= 0)
            box->setCurrentText(QStringLiteral("Cash"));
    for (QDoubleSpinBox *cash : cashier->findChildren<QDoubleSpinBox *>())
        if (cash->isVisible())
            cash->setValue(100000);
    QPushButton *pay = button(cashier, QStringLiteral("Generate bill"));
    check(pay, QStringLiteral("New Order: no Generate bill button."));
    if (!pay)
        return;
    pay->click();
    settle();

    QVector<Pos::Order> after;
    check(Pos::readOrders(QStringLiteral("Data/Order.txt"), after, error) && after.size() == before.size() + 1,
        QStringLiteral("The sale was not saved to Order.txt (%1).").arg(error));
    if (after.size() != before.size() + 1)
        return;
    const Pos::Order sale = after.last();
    check(sale.customerName == QStringLiteral("CI_Test") && sale.status == QStringLiteral("paid") &&
            sale.paymentMode.compare(QStringLiteral("cash"), Qt::CaseInsensitive) == 0,
        QStringLiteral("The saved sale has the wrong customer, status or payment mode."));
    check(stockOf(QStringLiteral("P0001")) == stockBefore - 1,
        QStringLiteral("Selling 1 item must reduce its stock by 1 (was %1, now %2).")
            .arg(stockBefore).arg(stockOf(QStringLiteral("P0001"))));

    QPushButton *history = button(cashier, QStringLiteral("Order History"));
    check(history, QStringLiteral("Cashier: no Order History button."));
    if (!history)
        return;
    history->click();
    settle();
    check(showsText(cashier, sale.id),
        QStringLiteral("Order History does not show the new order %1.").arg(sale.id));
}
}

int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("DUKAANOS_THEME", "light");   // start the same on every machine

    QApplication app(argc, argv);
    UiTheme::install(app);

    QTemporaryDir work;
    if (!work.isValid() ||
        !copyFolder(QDir(QStringLiteral(DUKAANOS_SOURCE_DIR)).filePath(QStringLiteral("Data")),
                    work.filePath(QStringLiteral("Data"))))
    {
        QTextStream(stderr) << "Could not copy Data/ to a temporary folder." << Qt::endl;
        return 1;
    }
    const QString startFolder = QDir::currentPath();
    QDir::setCurrent(work.path());   // the app reads Data/ from here
    closePopupsAutomatically();

    QWidget *window = createMainWindow();
    window->show();
    settle();
    auto *stack = qobject_cast<QStackedWidget *>(window);
    check(stack, QStringLiteral("createMainWindow must return the page stack."));
    if (!stack)
        return 1;

    testLogin(window);
    testRoles(stack);
    testSale(stack);

    delete window;
    QDir::setCurrent(startFolder);   // Windows cannot delete the current folder
    if (failures > 0)
    {
        QTextStream(stderr) << failures << " check(s) failed." << Qt::endl;
        return 1;
    }
    QTextStream(stdout) << "UI smoke test passed." << Qt::endl;
    return 0;
}
