// Owner: Umar Khan - opens the real screens without a display (offscreen) and
// clicks through them, so a page that crashes or a broken role lock is caught
// in the pull request. Works on a temporary copy of Data/, so the project's
// sample data is never changed.
//
// Covers: login checks (empty fields, wrong security answer); the pages each
// role may open (Manager / Cashier / unknown role); every page in light and
// dark mode; a cashier sale: hold, resume, pay in cash and export its PDF;
// manager access to the saved bill and PDF export.

#include "../ui.h"
#include "../theme.h"
#include "../pages/bill_page.h"
#include "../pages/dashboard_page.h"
#include "../pages/saved_bill.h"

#include "../../POS/tests/data_format_checker.h"
#include "../../POS/order/order_repository.h"
#include "../../POS/common/data_paths.h"
#include "../../POS/product/product_repository.h"
#include "../../POS/user/auth_service.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QDate>
#include <QDir>
#include <QDirIterator>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QEvent>
#include <QFile>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <QTableWidget>

#ifndef DUKAANOS_SOURCE_DIR
#error "DUKAANOS_SOURCE_DIR must point at the project folder (set in CMakeLists.txt)."
#endif

namespace
{
int failures = 0;
int billDialogsWithPdfButton = 0;

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

void waitFor(int milliseconds)
{
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
    settle();
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
        bool hasPdfButton = false;
        const QList<QPushButton *> buttons = popup->findChildren<QPushButton *>();
        for (QPushButton *b : buttons)
            hasPdfButton = hasPdfButton
                || b->text().trimmed() == QStringLiteral("Download PDF");
        if (hasPdfButton && !popup->property("pdfButtonCounted").toBool())
        {
            popup->setProperty("pdfButtonCounted", true);
            ++billDialogsWithPdfButton;
        }
        for (QPushButton *b : buttons)
            if (b->text().trimmed() == QStringLiteral("Close") || b->text().trimmed() == QStringLiteral("OK"))
            {
                b->click();
                return;
            }
        popup->close();
    });
    timer->start(50);
}

bool isPdfFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) && file.size() > 100
        && file.read(4) == QByteArrayLiteral("%PDF");
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
                                                QStringLiteral("Products"), QStringLiteral("Order History"),
                                                QStringLiteral("Cashier Accounts")}),
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
    check(!sidebarPages(cashier).contains(QStringLiteral("Cashier Accounts")),
        QStringLiteral("A cashier must not be able to open Cashier Accounts."));
    visitAllPages(cashier, QStringLiteral("Cashier"));

    QWidget *unknown = openDashboard(stack, QStringLiteral("admin"));
    check(sidebarPages(unknown) == cashierPages,
        QStringLiteral("An unknown role must get the least access (Cashier)."));
}

void testCashierAccountCreation(QStackedWidget *stack)
{
    QWidget *manager = openDashboard(stack, QStringLiteral("Manager"));
    QPushButton *accounts = button(manager, QStringLiteral("Cashier Accounts"));
    check(accounts, QStringLiteral("Manager has no Cashier Accounts page."));
    if (!accounts)
        return;
    accounts->click();
    settle();

    QLineEdit *username = field(manager, QStringLiteral("Enter username"));
    QLineEdit *password = field(manager, QStringLiteral("Enter password"));
    QLineEdit *confirmPassword = field(manager, QStringLiteral("Enter confirm password"));
    QPushButton *create = button(manager, QStringLiteral("Create account"));
    check(username && password && confirmPassword && create,
        QStringLiteral("Cashier account form is incomplete."));
    if (!username || !password || !confirmPassword || !create)
        return;

    username->setText(QStringLiteral("smoke_cashier"));
    password->setText(QStringLiteral("smoke123"));
    confirmPassword->setText(QStringLiteral("smoke123"));
    create->click();
    settle();

    Pos::User user;
    QString error;
    check(Pos::login(Pos::DataPaths::users(), QStringLiteral("smoke_cashier"),
            QStringLiteral("smoke123"), user, error)
        && user.role == QStringLiteral("cashier"),
        QStringLiteral("Created account cannot log in as a cashier: %1").arg(error));
}

void testSale(QStackedWidget *stack)
{
    QTemporaryDir pdfs;
    check(pdfs.isValid(), QStringLiteral("Could not create temporary PDF test folder."));
    if (!pdfs.isValid())
        return;

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
    check(customer && barcode, QStringLiteral("New Order: customer or barcode field missing."));
    check(!button(cashier, QStringLiteral("Add item")),
        QStringLiteral("New Order should add products without an Add item button."));
    if (!customer || !barcode)
        return;
    customer->setText(QStringLiteral("CI Test"));
    barcode->setText(QStringLiteral("100001"));
    waitFor(300);
    check(showsText(cashier, QStringLiteral("Unit price: PKR 250.00")),
        QStringLiteral("Entering a complete barcode must show product details before Enter."));
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(barcode, &enter);
    settle();
    check(showsText(cashier, QStringLiteral("100001")),
        QStringLiteral("Pressing Enter must add the matched barcode to the cart."));

    // Hold the order, then resume it from Held Orders.
    QPushButton *hold = button(cashier, QStringLiteral("Hold order"));
    check(hold, QStringLiteral("New Order: no Hold order button."));
    if (!hold)
        return;
    hold->click();
    settle();
    QVector<Pos::Order> heldSnapshot;
    check(Pos::readOrders(Pos::DataPaths::orders(), heldSnapshot, error)
            && heldSnapshot.size() == before.size() + 1,
        QStringLiteral("Holding an order must persist one order (%1).").arg(error));
    if (heldSnapshot.size() != before.size() + 1)
        return;
    const QString heldOrderId = heldSnapshot.last().id;
    check(heldSnapshot.last().status == QStringLiteral("on-hold"),
        QStringLiteral("A held order must be saved with on-hold status."));
    check(hasLabel(cashier, QStringLiteral("Held Orders")) && showsText(cashier, heldOrderId),
        QStringLiteral("Holding an order must open Held Orders and list the saved order ID."));

    // A newly created dashboard after sign-out must reload the held order from disk.
    cashier = openDashboard(stack, QStringLiteral("Cashier"));
    QPushButton *heldOrders = button(cashier, QStringLiteral("Held Orders"));
    check(heldOrders, QStringLiteral("Cashier: no Held Orders button after signing in again."));
    if (!heldOrders)
        return;
    heldOrders->click();
    settle();
    check(showsText(cashier, heldOrderId),
        QStringLiteral("Held order %1 was not restored after creating a fresh dashboard.").arg(heldOrderId));
    QPushButton *resume = nullptr;
    for (QTableWidget *table : cashier->findChildren<QTableWidget *>())
    {
        if (!table->isVisible())
            continue;
        for (int row = 0; row < table->rowCount(); ++row)
        {
            if (!table->item(row, 0) || table->item(row, 0)->text() != heldOrderId)
                continue;
            QWidget *actions = table->cellWidget(row, 5);
            if (actions)
                for (QPushButton *candidate : actions->findChildren<QPushButton *>())
                    if (candidate->text() == QStringLiteral("Resume"))
                        resume = candidate;
        }
    }
    check(resume, QStringLiteral("Held Orders: no Resume button."));
    if (!resume)
        return;
    resume->click();
    settle();
    check(hasLabel(cashier, QStringLiteral("New Order")),
        QStringLiteral("Resume must go back to New Order."));
    check(showsText(cashier, QStringLiteral("100001")),
        QStringLiteral("The resumed order did not restore its product and quantity."));

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
    const int cashierBillDialogsBefore = billDialogsWithPdfButton;
    pay->click();
    settle();
    check(billDialogsWithPdfButton == cashierBillDialogsBefore + 1,
        QStringLiteral("Cashier bill dialog has no Download PDF action."));

    QVector<Pos::Order> after;
    check(Pos::readOrders(QStringLiteral("Data/Order.txt"), after, error) && after.size() == before.size() + 1,
        QStringLiteral("The sale was not saved to Order.txt (%1).").arg(error));
    if (after.size() != before.size() + 1)
        return;
    const Pos::Order sale = after.last();
    OrderPreview cashierBill;
    check(UiPages::loadSavedBill(sale, cashierBill, error)
            && UiPages::writeBillPdf(cashierBill, pdfs.filePath(QStringLiteral("cashier.pdf")))
            && isPdfFile(pdfs.filePath(QStringLiteral("cashier.pdf"))),
        QStringLiteral("Cashier bill could not be exported as a valid PDF (%1).").arg(error));
    QString checkoutError;
    for (QLabel *label : cashier->findChildren<QLabel *>())
        if (label->isVisible() && (label->text().startsWith(QStringLiteral("Bill not saved:"))
                || label->text().startsWith(QStringLiteral("Add at least one product"))))
            checkoutError = label->text();
    check(sale.customerName == QStringLiteral("CI_Test") && sale.status == QStringLiteral("paid") &&
            sale.paymentMode.compare(QStringLiteral("cash"), Qt::CaseInsensitive) == 0,
        QStringLiteral("The saved sale has the wrong customer, status or payment mode "
            "(customer=%1, status=%2, mode=%3; %4).")
            .arg(sale.customerName, sale.status, sale.paymentMode, checkoutError));
    check(sale.id == heldOrderId,
        QStringLiteral("Resuming and paying must update order %1, not create %2.")
            .arg(heldOrderId, sale.id));
    check(stockOf(QStringLiteral("P0001")) == stockBefore - 1,
        QStringLiteral("Selling 1 item must reduce its stock by 1 (was %1, now %2).")
            .arg(stockBefore).arg(stockOf(QStringLiteral("P0001"))));

    int todayOrderCount = 0;
    double todaySales = 0;
    for (const Pos::Order &order : after)
    {
        if (order.date != QDate::currentDate().toString(Qt::ISODate))
            continue;
        ++todayOrderCount;
        if (order.status == QStringLiteral("paid"))
            todaySales += order.paymentAmount;
    }
    QPushButton *dashboard = button(cashier, QStringLiteral("Dashboard"));
    check(dashboard, QStringLiteral("Cashier: no Dashboard button."));
    if (dashboard)
    {
        dashboard->click();
        settle();
        check(showsText(cashier, QString::number(todayOrderCount)),
            QStringLiteral("Dashboard order count did not reflect today's saved orders."));
        check(showsText(cashier, QStringLiteral("PKR %1").arg(todaySales, 0, 'f', 2)),
            QStringLiteral("Dashboard sales did not reflect today's paid orders."));
    }

    QPushButton *history = button(cashier, QStringLiteral("Order History"));
    check(history, QStringLiteral("Cashier: no Order History button."));
    if (!history)
        return;
    history->click();
    settle();
    check(showsText(cashier, sale.id),
        QStringLiteral("Order History does not show the new order %1.").arg(sale.id));

    QWidget *manager = openDashboard(stack, QStringLiteral("Manager"));
    QPushButton *managerHistory = button(manager, QStringLiteral("Order History"));
    check(managerHistory, QStringLiteral("Manager: no Order History button."));
    if (!managerHistory)
        return;
    managerHistory->click();
    settle();
    QTableWidget *historyTable = nullptr;
    int saleRow = -1;
    for (QTableWidget *table : manager->findChildren<QTableWidget *>())
    {
        if (!table->isVisible())
            continue;
        for (int row = 0; row < table->rowCount(); ++row)
            if (table->item(row, 0) && table->item(row, 0)->text() == sale.id)
            {
                historyTable = table;
                saleRow = row;
                break;
            }
    }
    check(historyTable && saleRow >= 0,
        QStringLiteral("Manager Order History does not list the paid order."));
    if (!historyTable || saleRow < 0)
        return;
    historyTable->item(saleRow, 0)->setSelected(true);
    QPushButton *viewBill = button(manager, QStringLiteral("View bill"));
    check(viewBill && viewBill->isEnabled(),
        QStringLiteral("Manager cannot open the saved bill."));
    if (!viewBill || !viewBill->isEnabled())
        return;
    const int managerBillDialogsBefore = billDialogsWithPdfButton;
    viewBill->click();
    settle();
    check(billDialogsWithPdfButton == managerBillDialogsBefore + 1,
        QStringLiteral("Manager saved-bill dialog has no Download PDF action."));
    OrderPreview managerBill;
    check(UiPages::loadSavedBill(sale, managerBill, error)
            && UiPages::writeBillPdf(managerBill, pdfs.filePath(QStringLiteral("manager.pdf")))
            && isPdfFile(pdfs.filePath(QStringLiteral("manager.pdf"))),
        QStringLiteral("Manager bill could not be exported as a valid PDF (%1).").arg(error));
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
    testCashierAccountCreation(stack);
    testSale(stack);

    // The files the app has just written (sale, held order, stock change)
    // must follow the required formats, not only the sample data.
    DataFormat::Report formats;
    if (DataFormat::runAll(work.path(), formats) == DataFormat::Result::Failed)
        for (const QString &problem : formats.problems)
            check(false, QStringLiteral("File written by the app is in the wrong format: %1").arg(problem));

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
