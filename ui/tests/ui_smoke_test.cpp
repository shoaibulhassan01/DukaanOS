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
#include "../pages/new_order_page.h"
#include "../pages/product_dialog.h"
#include "../pages/seasons_page.h"
#include "../theme.h"
#include "../pages/bill_page.h"
#include "../pages/dashboard_page.h"
#include "../pages/saved_bill.h"

#include "../../POS/tests/data_format_checker.h"
#include "../../POS/checkout/checkout_service.h"
#include "../../POS/order/order_repository.h"
#include "../../POS/common/data_paths.h"
#include "../../POS/product/product_repository.h"
#include "../../POS/category/category_service.h"
#include "../../POS/product/product_service.h"
#include "../../POS/report/report_service.h"
#include "../../POS/user/auth_service.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QFileInfo>
#include <QFileDialog>
#include <QCompleter>
#include <QDate>
#include <QDir>
#include <QDirIterator>
#include <QDoubleSpinBox>
#include <QDialog>
#include <QEventLoop>
#include <QEvent>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTableWidget>
#include <QTimer>
#include <QValidator>

#include <functional>
#include <memory>

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

void checkBarcodeField(QWidget *root, const QString &page)
{
    QLineEdit *barcode = field(root, QStringLiteral("Enter product barcode"));
    check(barcode, QStringLiteral("%1: barcode field is missing.").arg(page));
    if (!barcode)
        return;

    check(barcode->maxLength() == 6 && barcode->validator(),
        QStringLiteral("%1: barcode entry must be limited to six digits.").arg(page));
    if (!barcode->validator())
        return;

    int position = 0;
    QString sixDigits = QStringLiteral("123456");
    check(barcode->validator()->validate(sixDigits, position) == QValidator::Acceptable,
        QStringLiteral("%1: valid six-digit barcodes must be accepted.").arg(page));
    QString nonDigits = QStringLiteral("12a456");
    position = 0;
    check(barcode->validator()->validate(nonDigits, position) == QValidator::Invalid,
        QStringLiteral("%1: non-digit barcode input must be rejected.").arg(page));
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
// In dark mode, every visible part of a page must have a dark background. A
// light colour the theme does not know stays light (e.g. a new icon tile);
// this finds it.
void checkNoLightBackgrounds(QWidget *root, const QString &where)
{
    static const QRegularExpression background(
        QStringLiteral("background(?:-color)?\\s*:\\s*(#[0-9A-Fa-f]{6}|white)\\b"));
    QStringList light;
    QList<QWidget *> widgets = root->findChildren<QWidget *>();
    widgets.prepend(root);   // a pop-up form's own background counts too
    for (QWidget *widget : widgets)
    {
        if (!widget->isVisible())
            continue;
        auto it = background.globalMatch(widget->styleSheet());
        while (it.hasNext())
        {
            const QColor color(it.next().captured(1));
            if (color.lightnessF() > 0.80)
                light << QStringLiteral("%1 (%2)").arg(color.name(),
                    widget->objectName().isEmpty() ? QString::fromLatin1(widget->metaObject()->className())
                                                   : widget->objectName());
        }
    }
    light.removeDuplicates();
    check(light.isEmpty(), QStringLiteral("%1: light background(s) in dark mode: %2")
        .arg(where, light.join(QStringLiteral(", "))));
}

void visitAllPages(QWidget *dashboard, const QString &who)
{
    if (UiTheme::isDark())
        checkNoLightBackgrounds(dashboard, who + QStringLiteral(" Dashboard"));
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
        if (page == QStringLiteral("New Order") || page == QStringLiteral("Scan Product"))
        {
            check(button(dashboard, QStringLiteral("Upload image")),
                QStringLiteral("%1 %2 page must offer barcode image upload.").arg(who, page));
            checkBarcodeField(dashboard, page);
            const QString action = page == QStringLiteral("New Order")
                ? QStringLiteral("Add item") : QStringLiteral("Look up");
            check(button(dashboard, action),
                QStringLiteral("%1 %2 page must offer a manual barcode action.").arg(who, page));
        }
        if (UiTheme::isDark())
            checkNoLightBackgrounds(dashboard, QStringLiteral("%1 %2").arg(who, page));
    }
}

// Clicks Close on any pop-up (bill dialog) that opens, so exec() returns.
// A test can set this to fill in and answer the next pop-up itself (for
// example the Add category form). It returns true while it is handling the
// pop-up; then the default Close/OK handling below is skipped.
std::function<bool(QWidget *)> popupHandler;

void closePopupsAutomatically()
{
    auto *timer = new QTimer(qApp);
    QObject::connect(timer, &QTimer::timeout, []() {
        QWidget *popup = QApplication::activeModalWidget();
        if (!popup)
            return;
        if (popupHandler && popupHandler(popup))
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

void approveManagerEdit()
{
    popupHandler = [](QWidget *popup) {
        if (popup->objectName() != QStringLiteral("managerApprovalDialog"))
            return false;
        auto *username = popup->findChild<QLineEdit *>(QStringLiteral("managerApprovalUsername"));
        auto *password = popup->findChild<QLineEdit *>(QStringLiteral("managerApprovalPassword"));
        QPushButton *approve = button(popup, QStringLiteral("Approve"));
        if (!username || !password || !approve)
            return false;
        username->setText(QStringLiteral("manager"));
        password->setText(QStringLiteral("manager123"));
        approve->click();
        popupHandler = nullptr;
        return true;
    };
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
                                                QStringLiteral("Products"), QStringLiteral("Categories"),
                                                QStringLiteral("Season Management"), QStringLiteral("Order History"),
                                                QStringLiteral("Reports"), QStringLiteral("Cashier Accounts")}),
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
    check(!sidebarPages(cashier).contains(QStringLiteral("Categories")),
        QStringLiteral("A cashier must not be able to open Categories."));
    check(!sidebarPages(cashier).contains(QStringLiteral("Reports")),
        QStringLiteral("A cashier must not be able to open Reports."));
    check(!sidebarPages(cashier).contains(QStringLiteral("Season Management")),
        QStringLiteral("A cashier must not be able to open Season Management."));
    visitAllPages(cashier, QStringLiteral("Cashier"));

    QWidget *unknown = openDashboard(stack, QStringLiteral("admin"));
    check(sidebarPages(unknown) == cashierPages,
        QStringLiteral("An unknown role must get the least access (Cashier)."));
}

QString fileText(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(file.readAll());
}

// Fills the Add category form in the next pop-up and presses "Add category".
// If the form is still open afterwards (the category was refused), records
// the error shown and presses Cancel.
void answerCategoryForm(const QString &id, const QString &name, const QString &acronym,
    QString *shownError)
{
    auto stage = std::make_shared<int>(0);
    popupHandler = [=](QWidget *popup) {
        QLineEdit *idField = field(popup, QStringLiteral("e.g. 10"));
        if (!idField)
            return false;
        if (*stage == 0)
        {
            idField->setText(id);
            field(popup, QStringLiteral("e.g. Beverages"))->setText(name);
            field(popup, QStringLiteral("e.g. BEV"))->setText(acronym);
            *stage = 1;
            if (QPushButton *add = button(popup, QStringLiteral("Add category")))
                add->click();
            return true;
        }
        for (QLabel *label : popup->findChildren<QLabel *>())
            if (label->isVisible() && label->styleSheet().contains(QStringLiteral("B42318")))
                *shownError = label->text();
        if (QPushButton *cancel = button(popup, QStringLiteral("Cancel")))
            cancel->click();
        popupHandler = nullptr;
        return true;
    };
}

// Categories page (Increment 3, req 1): lists the sample categories, adds one
// through the form, refuses a duplicate Id, refuses deleting a category that
// products use, deletes an unused one.
void testCategoriesPage(QStackedWidget *stack)
{
    const QString categoryFile = QStringLiteral("Data/PCategory.txt");
    QWidget *manager = openDashboard(stack, QStringLiteral("Manager"));
    QPushButton *page = button(manager, QStringLiteral("Categories"));
    check(page, QStringLiteral("Manager has no Categories page."));
    if (!page)
        return;
    page->click();
    settle();
    check(hasLabel(manager, QStringLiteral("Product categories")),
        QStringLiteral("The Categories page did not open."));
    const QString original = fileText(categoryFile);
    check(showsText(manager, QStringLiteral("Beverages")) && showsText(manager, QStringLiteral("Grains and pulses")),
        QStringLiteral("Categories page must list the categories in PCategory.txt."));

    QPushButton *add = button(manager, QStringLiteral("+ Add category"));
    check(add, QStringLiteral("Categories page: no Add category button."));
    if (!add)
        return;
    QString shownError;
    answerCategoryForm(QStringLiteral("90"), QStringLiteral("Frozen food"), QStringLiteral("FRZ"), &shownError);
    add->click();
    settle();
    popupHandler = nullptr;
    check(fileText(categoryFile) == original + QStringLiteral("90 Frozen_food FRZ#\n"),
        QStringLiteral("Adding a category through the form must append \"90 Frozen_food FRZ#\", got: %1")
            .arg(fileText(categoryFile)));
    check(showsText(manager, QStringLiteral("Frozen food")),
        QStringLiteral("The new category must appear in the table."));
    DataFormat::Report format;
    check(DataFormat::runGroup(QStringLiteral("PCategory"), QDir::currentPath(), format)
              == DataFormat::Result::Passed,
        QStringLiteral("PCategory.txt written by the Categories page is in the wrong format: %1")
            .arg(format.problems.join(QStringLiteral("; "))));

    // Same Id again: the form must refuse it and show why.
    shownError.clear();
    answerCategoryForm(QStringLiteral("10"), QStringLiteral("Dairy"), QStringLiteral("DRY"), &shownError);
    add->click();
    settle();
    popupHandler = nullptr;
    check(shownError.contains(QStringLiteral("already used")),
        QStringLiteral("A duplicate Category Id must be refused with a message, got: '%1'").arg(shownError));
    check(fileText(categoryFile) == original + QStringLiteral("90 Frozen_food FRZ#\n"),
        QStringLiteral("A refused duplicate must not change PCategory.txt."));

    // Search box: filters the table, and clearing it shows every category.
    QLineEdit *categorySearch = field(manager, QStringLiteral("Search by name, ID or acronym"));
    check(categorySearch, QStringLiteral("Categories page: no search box."));
    if (categorySearch)
    {
        auto visibleRows = [manager]() {
            for (QTableWidget *t : manager->findChildren<QTableWidget *>())
                if (t->isVisible())
                    return t->rowCount();
            return 0;
        };
        const int allRows = visibleRows();
        categorySearch->setText(QStringLiteral("frozen"));
        settle();
        check(visibleRows() == 1 && showsText(manager, QStringLiteral("Frozen food")),
            QStringLiteral("Searching 'frozen' must show only Frozen food, rows: %1").arg(visibleRows()));
        categorySearch->setText(QStringLiteral("zzz"));
        settle();
        check(visibleRows() == 0 && showsText(manager, QStringLiteral("No categories match your search.")),
            QStringLiteral("A search with no match must show an empty-search message."));
        categorySearch->clear();
        settle();
        check(visibleRows() == allRows, QStringLiteral("Clearing the search must show every category again."));
    }

    // Products 100001-100004 belong to category 10 (first row), so it cannot
    // be deleted; the new, unused category 90 (last row) can.
    QTableWidget *table = nullptr;
    for (QTableWidget *t : manager->findChildren<QTableWidget *>())
        if (t->isVisible())
            table = t;
    QPushButton *remove = button(manager, QStringLiteral("Delete"));
    check(table && remove, QStringLiteral("Categories page: no table or Delete button."));
    if (table && remove)
    {
        table->selectRow(0);
        popupHandler = [](QWidget *popup) {
            if (QPushButton *yes = button(popup, QStringLiteral("&Yes")))
            {
                yes->click();
                popupHandler = nullptr;   // the warning that follows is closed with OK
                return true;
            }
            return false;
        };
        remove->click();
        settle();
        popupHandler = nullptr;
        check(fileText(categoryFile) == original + QStringLiteral("90 Frozen_food FRZ#\n"),
            QStringLiteral("A category used by products must not be deleted."));

        table->selectRow(table->rowCount() - 1);
        popupHandler = [](QWidget *popup) {
            if (QPushButton *yes = button(popup, QStringLiteral("&Yes")))
            {
                yes->click();
                popupHandler = nullptr;
                return true;
            }
            return false;
        };
        remove->click();
        settle();
        popupHandler = nullptr;
        check(fileText(categoryFile) == original,
            QStringLiteral("Deleting the unused category 90 must remove only its line."));
    }

    // Products page: each product shows its category (first two digits of
    // the barcode), or "not found" when PCategory.txt has no such Id.
    QPushButton *productsPage = button(manager, QStringLiteral("Products"));
    check(productsPage, QStringLiteral("Manager has no Products page."));
    if (productsPage)
    {
        productsPage->click();
        settle();
        // The CATEGORY column shows the category name only.
        QTableWidget *productList = nullptr;
        for (QTableWidget *t : manager->findChildren<QTableWidget *>())
            if (t->isVisible())
                productList = t;
        int categoryColumn = -1;
        for (int c = 0; productList && c < productList->columnCount(); ++c)
            if (productList->horizontalHeaderItem(c) && productList->horizontalHeaderItem(c)->text() == QStringLiteral("CATEGORY"))
                categoryColumn = c;
        check(productList && productList->height() >= 302,
            QStringLiteral("Products inventory must show at least four product rows at once."));
        QStringList shown;
        for (int r = 0; productList && categoryColumn >= 0 && r < productList->rowCount(); ++r)
            if (productList->item(r, categoryColumn))
                shown << productList->item(r, categoryColumn)->text();
        check(shown.contains(QStringLiteral("Beverages")) && shown.contains(QStringLiteral("Snacks")) &&
                shown.contains(QStringLiteral("Grains and pulses")),
            QStringLiteral("Products page: the CATEGORY column must show category names, got: %1")
                .arg(shown.join(QStringLiteral(", "))));
        // Weight is shown in kg: Tea leaves (P0003) is sold by weight.
        for (int r = 0; productList && r < productList->rowCount(); ++r)
        {
            QWidget *nameCell = productList->cellWidget(r, 0);
            bool isTea = false;
            for (QLabel *label : nameCell ? nameCell->findChildren<QLabel *>() : QList<QLabel *>())
                isTea = isTea || label->text().contains(QStringLiteral("P0003"));
            if (!isTea)
                continue;
            check(productList->item(r, 3) && productList->item(r, 3)->text() == QStringLiteral("weight (kg)") &&
                    productList->item(r, 7) && productList->item(r, 7)->text().endsWith(QStringLiteral(" kg")),
                QStringLiteral("Products page: a weighed product must show unit 'weight (kg)' and stock in kg."));
        }
        check(!shown.contains(QStringLiteral("Not found")) && !shown.join(QString()).contains(QStringLiteral("\u00B7")),
            QStringLiteral("Products page: every sample product has a category, shown by name only, got: %1")
                .arg(shown.join(QStringLiteral(", "))));

        // Add product form: the category appears as the barcode is typed, and
        // a product whose category does not exist cannot be saved.
        QStringList seen;
        QString expectedId;
        QString idError;
        Pos::nextProductId(QStringLiteral("Data/Product.txt"), expectedId, idError);
        QString shownId;
        bool suggestedAcronym = false;
        bool preservedManualAcronym = false;
        bool barcodePreviewReady = false;
        QString searchResults;
        popupHandler = [&seen, &shownId, &suggestedAcronym, &preservedManualAcronym,
                           &barcodePreviewReady, &searchResults](QWidget *popup) {
            QComboBox *category = popup->findChild<QComboBox *>(QStringLiteral("productCategory"));
            QLineEdit *barcode = field(popup, QStringLiteral("Choose a category first"));
            if (!category || !barcode)
                return false;
            // seen: 0 picker items, 1 barcode before a choice, 2 after
            // choosing 10, 3 after trying to delete/type over "10",
            // 4 after typing the rest, 5 after switching to 20.
            QStringList items;
            for (int i = 0; i < category->count(); ++i)
                items << category->itemText(i);
            seen << items.join(QStringLiteral(" | "));
            seen << QStringLiteral("%1/%2").arg(barcode->isEnabled() ? QStringLiteral("enabled")
                                                                     : QStringLiteral("disabled"),
                        barcode->text());
            // Search in the picker: by Id, by part of a name, or nothing found.
            QStringList searched;
            auto search = [category, &searched](const QString &text) {
                category->lineEdit()->setText(text);
                emit category->lineEdit()->editingFinished();
                searched << category->currentData().toString();
            };
            category->completer()->setCompletionPrefix(QStringLiteral("4"));
            searched << QString::number(category->completer()->completionCount());
            search(QStringLiteral("20"));        // Id typed in full
            search(QStringLiteral("dai"));       // only Dairy contains it
            search(QStringLiteral("zzz"));       // no match: Dairy stays
            searchResults = searched.join(QLatin1Char(','));
            category->setCurrentIndex(category->findData(QStringLiteral("10")));
            seen << barcode->text();
            barcode->setCursorPosition(2);
            barcode->backspace();             // would delete the "0" of "10"
            barcode->setCursorPosition(0);    // cursor is kept after "10"
            barcode->insert(QStringLiteral("9"));
            seen << barcode->text();
            barcode->insert(QStringLiteral("009"));
            seen << barcode->text();
            category->setCurrentIndex(category->findData(QStringLiteral("20")));
            seen << barcode->text();
            for (QLineEdit *edit : popup->findChildren<QLineEdit *>())
                if (edit->toolTip().startsWith(QStringLiteral("Assigned automatically")))
                    shownId = edit->isEnabled() ? QStringLiteral("editable") : edit->text();
            field(popup, QStringLiteral("e.g. Green tea"))->setText(QStringLiteral("CI item"));
            suggestedAcronym = field(popup, QStringLiteral("Up to 8 characters"))->text()
                == QStringLiteral("CII");
            field(popup, QStringLiteral("Up to 8 characters"))->setText(QStringLiteral("CI"));
            field(popup, QStringLiteral("e.g. Green tea"))->setText(QStringLiteral("Changed name"));
            preservedManualAcronym = field(popup, QStringLiteral("Up to 8 characters"))->text()
                == QStringLiteral("CI");
            field(popup, QStringLiteral("e.g. Green tea"))->setText(QStringLiteral("CI item"));
            QLabel *preview = popup->findChild<QLabel *>(QStringLiteral("barcodePreview"));
            QPushButton *download = button(popup, QStringLiteral("Download barcode"));
            barcodePreviewReady = preview && preview->text().isEmpty()
                && download && download->isEnabled();
            for (QLineEdit *money : popup->findChildren<QLineEdit *>())
                if (money->placeholderText() == QStringLiteral("e.g. 250.00"))
                    money->setText(QStringLiteral("1.00"));
            for (QLineEdit *quantity : popup->findChildren<QLineEdit *>())
                if (quantity->placeholderText() == QStringLiteral("e.g. 10"))
                    quantity->setText(QStringLiteral("5"));
            if (QPushButton *cancel = button(popup, QStringLiteral("Cancel")))
                cancel->click();
            popupHandler = nullptr;
            return true;
        };
        if (QPushButton *addProduct = button(manager, QStringLiteral("+ Add product")))
            addProduct->click();
        settle();
        popupHandler = nullptr;
        check(seen.size() == 6, QStringLiteral("Add product form did not open (or has no category picker)."));
        if (seen.size() == 6)
        {
            check(seen.at(0).startsWith(QStringLiteral("10 \u00B7 Beverages (BEV) | 20 \u00B7 Bakery (BKY)")),
                QStringLiteral("The category picker must list every category by Id, saw: %1").arg(seen.at(0)));
            // "4" is in 34, 40 and 45; "20" picks Bakery; "dai" picks Dairy;
            // "zzz" finds nothing and keeps Dairy.
            check(searchResults == QStringLiteral("3,20,56,56"),
                QStringLiteral("Category search by Id or name gave: %1 (expected 3,20,56,56)").arg(searchResults));
            check(seen.at(1) == QStringLiteral("disabled/"),
                QStringLiteral("The barcode must stay empty and locked until a category is chosen, saw: %1").arg(seen.at(1)));
            check(seen.at(2) == QStringLiteral("10"),
                QStringLiteral("Choosing category 10 must start the barcode with 10, saw: %1").arg(seen.at(2)));
            check(seen.at(3) == QStringLiteral("109"),
                QStringLiteral("The category digits must not be deletable or typed over, saw: %1").arg(seen.at(3)));
            check(seen.at(4) == QStringLiteral("109009"),
                QStringLiteral("Only 4 digits can follow the category Id, saw: %1").arg(seen.at(4)));
            check(seen.at(5) == QStringLiteral("209009"),
                QStringLiteral("Changing the category must change the first two digits and keep the rest, saw: %1").arg(seen.at(5)));
        }
        // The product id is not typed: the form shows the next one, locked.
        check(shownId == expectedId,
            QStringLiteral("Add product must show the next product id %1, locked; got: %2")
                .arg(expectedId, shownId));
        check(suggestedAcronym,
            QStringLiteral("Add product must suggest the first three name characters as the acronym."));
        check(preservedManualAcronym,
            QStringLiteral("Changing the name must not overwrite a manually changed acronym."));
        check(barcodePreviewReady,
            QStringLiteral("Add product must show the generated barcode and enable its download."));
        // Edit product form: shows the category stored for the product.
        QString editSaw;
        bool editPreviewReady = false;
        int editableMoney = 0;
        QTableWidget *productTable = nullptr;
        for (QTableWidget *t : manager->findChildren<QTableWidget *>())
            if (t->isVisible())
                productTable = t;
        QPushButton *editProduct = button(manager, QStringLiteral("Edit selected"));
        if (productTable && editProduct)
        {
            productTable->selectRow(0);   // P0001 Apple juice, category 10
            popupHandler = [&editSaw, &editPreviewReady, &editableMoney](QWidget *popup) {
                if (!button(popup, QStringLiteral("Save changes")))
                    return false;
                QLabel *preview = popup->findChild<QLabel *>(QStringLiteral("barcodePreview"));
                QPushButton *download = button(popup, QStringLiteral("Download barcode"));
                editPreviewReady = preview && preview->text().isEmpty()
                    && download && download->isEnabled();
                if (QComboBox *category = popup->findChild<QComboBox *>(QStringLiteral("productCategory")))
                    editSaw = category->currentText()
                        + (category->isEnabled() ? QStringLiteral(" (editable)") : QString());
                // Price, tax and discount can be edited (req 6).
                for (QLineEdit *money : popup->findChildren<QLineEdit *>())
                    if (money->placeholderText() == QStringLiteral("e.g. 250.00") && money->isEnabled())
                        ++editableMoney;
                if (QPushButton *cancel = button(popup, QStringLiteral("Cancel")))
                    cancel->click();
                popupHandler = nullptr;
                return true;
            };
            editProduct->click();
            settle();
            popupHandler = nullptr;
        }
        check(editSaw == QStringLiteral("10 \u00B7 Beverages (BEV)"),
            QStringLiteral("Edit product form must show the stored category, locked; saw: %1").arg(editSaw));
        check(editPreviewReady,
            QStringLiteral("Edit product must use the Add product layout with its barcode preview."));
        check(editableMoney == 3,
            QStringLiteral("Edit product: price, tax and discount must be editable, %1 of 3 are.").arg(editableMoney));

        QVector<Pos::Product> products;
        QString error;
        Pos::readProducts(QStringLiteral("Data/Product.txt"), products, error);
        bool added = false;
        for (const Pos::Product &product : products)
            added = added || product.barcode == QStringLiteral("209009");
        check(!added, QStringLiteral("A cancelled Add product form must not save anything."));

        // A product added as "weight (kg)" is saved as "weight" in Product.txt.
        QStringList unitChoices;
        popupHandler = [&unitChoices](QWidget *popup) {
            QComboBox *category = popup->findChild<QComboBox *>(QStringLiteral("productCategory"));
            QLineEdit *barcode = field(popup, QStringLiteral("Choose a category first"));
            if (!category || !barcode)
                return false;
            category->setCurrentIndex(category->findData(QStringLiteral("10")));
            barcode->insert(QStringLiteral("0098"));
            field(popup, QStringLiteral("e.g. Green tea"))->setText(QStringLiteral("Sugar"));
            field(popup, QStringLiteral("Up to 8 characters"))->setText(QStringLiteral("SGR"));
            if (QComboBox *unit = popup->findChild<QComboBox *>(QStringLiteral("productUnit")))
            {
                for (int i = 0; i < unit->count(); ++i)
                    unitChoices << unit->itemText(i);
                unit->setCurrentIndex(unit->findText(QStringLiteral("weight (kg)")));
            }
            for (QLineEdit *money : popup->findChildren<QLineEdit *>())
                if (money->placeholderText() == QStringLiteral("e.g. 250.00"))
                    money->setText(QStringLiteral("1.00"));
            for (QLineEdit *quantity : popup->findChildren<QLineEdit *>())
                if (quantity->placeholderText() == QStringLiteral("e.g. 10"))
                    quantity->setText(QStringLiteral("2"));
            if (QPushButton *save = button(popup, QStringLiteral("Add product")))
                save->click();
            popupHandler = nullptr;
            return true;
        };
        if (QPushButton *addProduct = button(manager, QStringLiteral("+ Add product")))
            addProduct->click();
        settle();
        popupHandler = nullptr;
        check(unitChoices == QStringList({QStringLiteral("count"), QStringLiteral("weight (kg)")}),
            QStringLiteral("Add product: units must be offered as 'count' and 'weight (kg)', got: %1")
                .arg(unitChoices.join(QStringLiteral(", "))));
        check(fileText(QStringLiteral("Data/Product.txt")).contains(
                  expectedId + QStringLiteral(" 100098 SGR Sugar weight 1.00 1.00 1.00 2 Data/barcodes/sugar_100098.png 10#")),
            QStringLiteral("A product added as 'weight (kg)' must be saved with unit 'weight' and the next id %1 in Product.txt.")
                .arg(expectedId));
        QString afterId;
        Pos::nextProductId(QStringLiteral("Data/Product.txt"), afterId, error);
        check(afterId == QStringLiteral("P%1").arg(expectedId.mid(1).toInt() + 1, 4, 10, QLatin1Char('0')),
            QStringLiteral("After adding %1 the next product id must go up by one, got %2.").arg(expectedId, afterId));

        // "+ Add new category" in the Add product form opens the Categories
        // page's form; once saved, the new category is selected and its Id
        // starts the barcode.
        const QString categoriesBefore = fileText(categoryFile);
        QString linkText;
        QString selectedAfter;
        QString barcodeAfter;
        popupHandler = [&](QWidget *popup) {
            QComboBox *category = popup->findChild<QComboBox *>(QStringLiteral("productCategory"));
            if (!category)
                return false;
            QPushButton *create = nullptr;
            for (QPushButton *b : popup->findChildren<QPushButton *>())
                if (b->text().startsWith(QStringLiteral("+ Add new category")))
                    create = b;
            linkText = create && create->isVisible() ? create->text() : QStringLiteral("(no visible link)");
            if (create)
            {
                // The category form opens inside the product form: answer it
                // from a separate timer.
                QTimer::singleShot(200, []() {
                    QWidget *form = QApplication::activeModalWidget();
                    QLineEdit *id = form ? field(form, QStringLiteral("e.g. 10")) : nullptr;
                    if (!id)
                        return;
                    id->setText(QStringLiteral("91"));
                    field(form, QStringLiteral("e.g. Beverages"))->setText(QStringLiteral("Imported goods"));
                    field(form, QStringLiteral("e.g. BEV"))->setText(QStringLiteral("IMP"));
                    if (QPushButton *add = button(form, QStringLiteral("Add category")))
                        add->click();
                });
                create->click();
            }
            selectedAfter = category->currentText();
            if (QLineEdit *barcode = field(popup, QStringLiteral("e.g. 100004")))
                barcodeAfter = barcode->text();
            if (QPushButton *cancel = button(popup, QStringLiteral("Cancel")))
                cancel->click();
            popupHandler = nullptr;   // last: this clears the lambda itself
            return true;
        };
        if (QPushButton *addProduct = button(manager, QStringLiteral("+ Add product")))
            addProduct->click();
        settle();
        popupHandler = nullptr;
        check(linkText == QStringLiteral("+ Add new category\u2026"),
            QStringLiteral("Add product must offer '+ Add new category...', got: %1").arg(linkText));
        check(fileText(categoryFile) == categoriesBefore + QStringLiteral("91 Imported_goods IMP#\n"),
            QStringLiteral("A category added from the product form must be appended to PCategory.txt."));
        check(selectedAfter == QStringLiteral("91 \u00B7 Imported goods (IMP)") && barcodeAfter == QStringLiteral("91"),
            QStringLiteral("The new category must be selected and start the barcode, saw: %1 / %2")
                .arg(selectedAfter, barcodeAfter));
        Pos::deleteCategory(categoryFile, QStringLiteral("Data/Product.txt"), QStringLiteral("91"), error);
    }

}

// Reports page (Increment 3, req 11): both of Umar's reports show what the
// report logic (tested in ReportTest) calculates from the sample data.
// Pop-up forms in dark mode: the Add / Edit product forms (with the barcode
// preview filled in), the Add category form opened from them, and the Add
// season form. Their
// colours are not covered by visiting the pages.
void testFormsInDarkMode()
{
    UiTheme::setDark(true);
    settle();
    QStringList opened;
    popupHandler = [&opened](QWidget *popup) {
        QComboBox *category = popup->findChild<QComboBox *>(QStringLiteral("productCategory"));
        if (!category || !button(popup, QStringLiteral("Add product")))
            return false;
        opened << QStringLiteral("Add product");
        checkNoLightBackgrounds(popup, QStringLiteral("Add product form (empty)"));
        category->setCurrentIndex(category->findData(QStringLiteral("10")));
        if (QLineEdit *barcode = field(popup, QStringLiteral("e.g. 100004")))
            barcode->insert(QStringLiteral("0777"));
        field(popup, QStringLiteral("e.g. Green tea"))->setText(QStringLiteral("Dark tea"));
        settle();
        checkNoLightBackgrounds(popup, QStringLiteral("Add product form (with barcode preview)"));
        for (QPushButton *b : popup->findChildren<QPushButton *>())
            if (b->text().startsWith(QStringLiteral("+ Add new category")))
            {
                // The category form opens inside the product form: check it
                // from a separate timer, then cancel it.
                QTimer::singleShot(200, [&opened]() {
                    QWidget *form = QApplication::activeModalWidget();
                    if (!form || !field(form, QStringLiteral("e.g. 10")))
                        return;
                    opened << QStringLiteral("Add category");
                    checkNoLightBackgrounds(form, QStringLiteral("Add category form"));
                    if (QPushButton *cancel = button(form, QStringLiteral("Cancel")))
                        cancel->click();
                });
                b->click();
            }
        if (QPushButton *cancel = button(popup, QStringLiteral("Cancel")))
            cancel->click();
        popupHandler = nullptr;   // last: this clears the lambda itself
        return true;
    };
    UiPages::showAddProductDialog(nullptr, QStringLiteral("Data/Product.txt"));
    settle();

    QVector<Pos::Product> products;
    QString error;
    Pos::readProducts(QStringLiteral("Data/Product.txt"), products, error);
    if (!products.isEmpty())
    {
        popupHandler = [&opened](QWidget *popup) {
            if (!button(popup, QStringLiteral("Save changes")))
                return false;
            opened << QStringLiteral("Edit product");
            checkNoLightBackgrounds(popup, QStringLiteral("Edit product form"));
            if (QPushButton *cancel = button(popup, QStringLiteral("Cancel")))
                cancel->click();
            popupHandler = nullptr;
            return true;
        };
        UiPages::showEditProductDialog(nullptr, QStringLiteral("Data/Product.txt"), products.first());
        settle();
    }
    popupHandler = nullptr;

    // Add season form (Seasons page), cancelled without saving.
    QWidget *seasons = UiPages::createSeasonsPage();
    seasons->show();
    settle();
    popupHandler = [&opened](QWidget *popup) {
        if (!button(popup, QStringLiteral("Add season")))
            return false;
        opened << QStringLiteral("Add season");
        checkNoLightBackgrounds(popup, QStringLiteral("Add season form"));
        if (QPushButton *cancel = button(popup, QStringLiteral("Cancel")))
            cancel->click();
        popupHandler = nullptr;
        return true;
    };
    if (QPushButton *addSeason = button(seasons, QStringLiteral("+ Add Season")))
        addSeason->click();
    settle();
    popupHandler = nullptr;
    delete seasons;

    check(opened == QStringList({ QStringLiteral("Add product"), QStringLiteral("Add category"),
                                  QStringLiteral("Edit product"), QStringLiteral("Add season") }),
        QStringLiteral("Dark-mode form check: expected Add product, Add category and Edit product to open, got: %1")
            .arg(opened.join(QStringLiteral(", "))));
    UiTheme::setDark(false);
    settle();
}

void testReportsPage(QStackedWidget *stack)
{
    QWidget *manager = openDashboard(stack, QStringLiteral("Manager"));
    QPushButton *page = button(manager, QStringLiteral("Reports"));
    check(page, QStringLiteral("Manager has no Reports page."));
    if (!page)
        return;
    page->click();
    settle();
    QComboBox *selector = manager->findChild<QComboBox *>(QStringLiteral("reportSelector"));
    QTableWidget *table = nullptr;
    for (QTableWidget *t : manager->findChildren<QTableWidget *>())
        if (t->isVisible())
            table = t;
    check(selector && table, QStringLiteral("Reports page: no report list or table."));
    if (!selector || !table)
        return;

    auto cell = [table](int row, int column) {
        QTableWidgetItem *item = table->item(row, column);
        return item ? item->text() : QString();
    };
    QString error;

    selector->setCurrentIndex(selector->findText(QStringLiteral("Top 10 most sold items")));
    settle();
    QVector<Pos::SoldItem> sold;
    Pos::topSoldProducts(QStringLiteral("Data/Product.txt"), QStringLiteral("Data/Order.txt"),
        QStringLiteral("Data/OrderDetail.txt"), 10, sold, error);
    check(!sold.isEmpty() && table->rowCount() == sold.size(),
        QStringLiteral("Top sold: the table must have %1 rows, has %2.").arg(sold.size()).arg(table->rowCount()));
    for (int row = 0; row < qMin(sold.size(), table->rowCount()); ++row)
    {
        const Pos::SoldItem &item = sold.at(row);
        check(cell(row, 0) == QString::number(row + 1) &&
                cell(row, 1).contains(item.product.id) &&
                cell(row, 3).startsWith(QString::number(item.quantitySold) + QLatin1Char(' ')) &&
                cell(row, 4).startsWith(QString::number(item.product.quantity) + QLatin1Char(' ')),
            QStringLiteral("Top sold row %1 shows '%2 | %3 | %4', expected %5 sold %6, stock %7.")
                .arg(row + 1).arg(cell(row, 1), cell(row, 3), cell(row, 4), item.product.id)
                .arg(item.quantitySold).arg(item.product.quantity));
    }
    check(cell(0, 2) == QStringLiteral("Beverages"),
        QStringLiteral("Reports: the CATEGORY column must show the category name only, got '%1'.").arg(cell(0, 2)));
    check(table->horizontalHeaderItem(3) && table->horizontalHeaderItem(3)->text() == QStringLiteral("QUANTITY SOLD") &&
            table->horizontalHeaderItem(4) && table->horizontalHeaderItem(4)->text() == QStringLiteral("REMAINING STOCK"),
        QStringLiteral("Top sold must show QUANTITY SOLD and REMAINING STOCK columns."));

    selector->setCurrentIndex(selector->findText(QStringLiteral("Top 10 items by remaining stock")));
    settle();
    QVector<Pos::Product> stock;
    Pos::topStockProducts(QStringLiteral("Data/Product.txt"), 10, stock, error);
    check(!stock.isEmpty() && table->rowCount() == stock.size(),
        QStringLiteral("Top stock: the table must have %1 rows, has %2.").arg(stock.size()).arg(table->rowCount()));
    for (int row = 0; row < qMin(stock.size(), table->rowCount()); ++row)
        check(cell(row, 1).contains(stock.at(row).id) &&
                cell(row, 3).startsWith(QString::number(stock.at(row).quantity) + QLatin1Char(' ')),
            QStringLiteral("Top stock row %1 shows '%2 | %3', expected %4 with %5.")
                .arg(row + 1).arg(cell(row, 1), cell(row, 3), stock.at(row).id).arg(stock.at(row).quantity));

    // Unit filter: counts and kilograms ranked separately.
    QComboBox *units = manager->findChild<QComboBox *>(QStringLiteral("reportUnitFilter"));
    check(units && units->count() == 3 && units->itemText(0) == QStringLiteral("All items") &&
            units->findText(QStringLiteral("Counted items")) >= 0 && units->findText(QStringLiteral("Weighed items (kg)")) >= 0,
        QStringLiteral("Reports: the unit filter must offer All items, Counted items and Weighed items (kg)."));
    if (!units)
        return;
    units->setCurrentIndex(units->findText(QStringLiteral("Weighed items (kg)")));
    settle();
    Pos::topStockProducts(QStringLiteral("Data/Product.txt"), 10, stock, error, QStringLiteral("weight"));
    check(!stock.isEmpty() && table->rowCount() == stock.size(),
        QStringLiteral("Weighed items: %1 rows expected, %2 shown.").arg(stock.size()).arg(table->rowCount()));
    for (int row = 0; row < table->rowCount(); ++row)
        check(cell(row, 3).endsWith(QStringLiteral(" kg")) && cell(row, 1).contains(stock.value(row).id),
            QStringLiteral("Weighed items row %1 must be %2 in kg, shows '%3 | %4'.")
                .arg(row + 1).arg(stock.value(row).id, cell(row, 1), cell(row, 3)));
    units->setCurrentIndex(units->findText(QStringLiteral("Counted items")));
    settle();
    for (int row = 0; row < table->rowCount(); ++row)
        check(cell(row, 3).endsWith(QStringLiteral(" count")),
            QStringLiteral("Counted items row %1 must be a count, shows '%2'.").arg(row + 1).arg(cell(row, 3)));
    selector->setCurrentIndex(selector->findText(QStringLiteral("Top 10 most sold items")));
    settle();
    check(table->rowCount() > 0, QStringLiteral("Most sold, counted items: rows expected."));
    for (int row = 0; row < table->rowCount(); ++row)
        check(cell(row, 3).endsWith(QStringLiteral(" count")) && cell(row, 4).endsWith(QStringLiteral(" count")),
            QStringLiteral("Most sold, counted items row %1 shows '%2 | %3'.").arg(row + 1).arg(cell(row, 3), cell(row, 4)));
    units->setCurrentIndex(0);
    settle();
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

    QTableWidget *accountsTable = nullptr;
    for (QTableWidget *table : manager->findChildren<QTableWidget *>())
        if (table->objectName() == QStringLiteral("cashierAccountsTable"))
            accountsTable = table;
    check(accountsTable && accountsTable->rowCount() > 0,
        QStringLiteral("Cashier Accounts must list the existing cashier accounts."));
    if (!accountsTable)
        return;

    auto accountRow = [accountsTable](const QString &accountName) {
        for (int row = 0; row < accountsTable->rowCount(); ++row)
            if (accountsTable->item(row, 0)
                && accountsTable->item(row, 0)->text() == accountName)
                return row;
        return -1;
    };
    int row = accountRow(QStringLiteral("smoke_cashier"));
    check(row >= 0, QStringLiteral("New cashier account was not added to the account list."));
    if (row < 0)
        return;

    auto editCashier = [&](int accountRow, const QString &newName,
                           const QString &newPassword) {
        auto *edit = qobject_cast<QPushButton *>(accountsTable->cellWidget(accountRow, 1));
        check(edit && edit->text() == QStringLiteral("Edit"),
            QStringLiteral("Each cashier row must provide an Edit action."));
        if (!edit)
            return false;

        bool dialogShown = false;
        bool fieldsAvailable = false;
        QTimer::singleShot(0, [&]() {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog || dialog->objectName() != QStringLiteral("editCashierAccountDialog"))
                return;
            dialogShown = true;
            auto *usernameField = dialog->findChild<QLineEdit *>(
                QStringLiteral("cashierEditUsername"));
            auto *passwordField = dialog->findChild<QLineEdit *>(
                QStringLiteral("cashierEditPassword"));
            auto *save = dialog->findChild<QPushButton *>(
                QStringLiteral("saveCashierEditButton"));
            fieldsAvailable = usernameField && passwordField && save
                && passwordField->echoMode() == QLineEdit::Password;
            if (!fieldsAvailable)
            {
                dialog->reject();
                return;
            }
            usernameField->setText(newName);
            passwordField->setText(newPassword);
            save->click();
        });
        edit->click();
        check(dialogShown, QStringLiteral("Clicking Edit must open the cashier edit popup."));
        check(fieldsAvailable,
            QStringLiteral("Edit popup must provide username and masked password fields."));
        return dialogShown && fieldsAvailable;
    };

    check(editCashier(row, QStringLiteral("smoke_cashier_renamed"), QString()),
        QStringLiteral("Could not edit cashier username from the popup."));
    settle();
    check(!Pos::login(Pos::DataPaths::users(), QStringLiteral("smoke_cashier"),
            QStringLiteral("smoke123"), user, error)
            && Pos::login(Pos::DataPaths::users(), QStringLiteral("smoke_cashier_renamed"),
                QStringLiteral("smoke123"), user, error),
        QStringLiteral("Cashier username change must preserve the existing password."));

    row = accountRow(QStringLiteral("smoke_cashier_renamed"));
    check(row >= 0, QStringLiteral("Renamed cashier row was not refreshed."));
    if (row < 0)
        return;
    check(editCashier(row, QStringLiteral("smoke_cashier_renamed"),
            QStringLiteral("smoke456")),
        QStringLiteral("Could not edit cashier password from the popup."));
    settle();
    check(!Pos::login(Pos::DataPaths::users(), QStringLiteral("smoke_cashier_renamed"),
            QStringLiteral("smoke123"), user, error)
            && Pos::login(Pos::DataPaths::users(), QStringLiteral("smoke_cashier_renamed"),
                QStringLiteral("smoke456"), user, error),
        QStringLiteral("Cashier password change must replace the old password."));
}

void testSale(QStackedWidget *stack)
{
    QTemporaryDir pdfs;
    check(pdfs.isValid(), QStringLiteral("Could not create temporary PDF test folder."));
    if (!pdfs.isValid())
        return;

    QFile seasons(Pos::DataPaths::seasons());
    check(seasons.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate),
        QStringLiteral("Could not prepare an active season for the sale test."));
    if (seasons.isOpen())
    {
        QTextStream seasonStream(&seasons);
        seasonStream << QDate::currentDate().addDays(-30).toString(Qt::ISODate)
                     << QLatin1Char(' ')
                     << QDate::currentDate().addDays(30).toString(Qt::ISODate)
                     << QStringLiteral(" 10 true#\n");
        seasons.close();
    }

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
    QPushButton *addItem = button(cashier, QStringLiteral("Add item"));
    check(addItem, QStringLiteral("New Order: no Add item button beside the barcode field."));
    QPushButton *hold = button(cashier, QStringLiteral("Hold order"));
    check(hold && !hold->isEnabled(),
        QStringLiteral("Hold order should be visible but disabled while the cart is empty."));
    if (!customer || !barcode)
        return;
    customer->setText(QStringLiteral("CI Test"));
    barcode->setText(QStringLiteral("100001"));
    waitFor(400);
    check(hold && !hold->isEnabled(),
        QStringLiteral("Typing a barcode must not add it until Add item is clicked."));
    if (addItem)
        addItem->click();
    settle();
    check(showsText(cashier, QStringLiteral("100001")),
        QStringLiteral("Clicking Add item must add the matched product."));
    check(barcode->text() == QStringLiteral("100001"),
        QStringLiteral("Add item must keep the barcode in the field, saw: %1").arg(barcode->text()));
    // Backspace (the user deleting it) is what clears it.
    barcode->setFocus();
    barcode->end(false);
    for (int i = 0; i < 6; ++i)
        barcode->backspace();
    check(barcode->text().isEmpty(),
        QStringLiteral("Backspace must clear the kept barcode, saw: %1").arg(barcode->text()));

    // A scanned barcode (here an uploaded barcode image) is added to the
    // order AND stays in the barcode field with its product shown, so the
    // cashier can open its description (same as a typed one). Uses a separate New Order page so
    // this sale's cart is not changed.
    {
        QVector<Pos::Product> productList;
        Pos::readProducts(Pos::DataPaths::products(), productList, error);
        QString imagePath;
        for (const Pos::Product &product : productList)
            if (product.barcode == QStringLiteral("100001"))
                imagePath = QFileInfo(product.barcodeImage).absoluteFilePath();
        QWidget *scanPage = UiPages::createNewOrderPage();
        scanPage->resize(1200, 800);
        scanPage->show();
        settle();
        popupHandler = [imagePath](QWidget *popup) {
            auto *chooser = qobject_cast<QFileDialog *>(popup);
            if (!chooser)
                return false;
            // Type the full path into the file name box, as a user would.
            if (QLineEdit *name = chooser->findChild<QLineEdit *>(QStringLiteral("fileNameEdit")))
                name->setText(imagePath);
            QMetaObject::invokeMethod(chooser, "accept");
            popupHandler = nullptr;   // last: this clears the lambda itself
            return true;
        };
        if (QPushButton *upload = button(scanPage, QStringLiteral("Upload image")))
            upload->click();
        settle();
        popupHandler = nullptr;
        QLineEdit *scanField = field(scanPage, QStringLiteral("Enter product barcode"));
        QTableWidget *scanCart = nullptr;
        for (QTableWidget *table : scanPage->findChildren<QTableWidget *>())
            if (table->isVisible() && table->columnCount() == 5)
                scanCart = table;
        check(scanCart && scanCart->rowCount() == 1 && showsText(scanPage, QStringLiteral("100001")),
            QStringLiteral("A scanned barcode must still be added to the order."));
        check(scanField && scanField->text() == QStringLiteral("100001"),
            QStringLiteral("A scanned barcode must stay in the barcode field, saw: %1")
                .arg(scanField ? scanField->text() : QStringLiteral("(no field)")));
        bool matchShown = false;
        for (QLabel *label : scanPage->findChildren<QLabel *>())
            matchShown = matchShown
                || (label->isVisible() && label->text().startsWith(QStringLiteral("<b>Apple juice</b>")));
        check(matchShown,
            QStringLiteral("The scanned product must stay shown under the barcode field."));
        delete scanPage;
    }
    check(hold && hold->isEnabled(),
        QStringLiteral("Hold order should become available as soon as a product is added."));

    // Hold the order, then resume it from Held Orders.
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

    // Order History opens an on-hold order directly in edit mode, without a
    // manager approval prompt, and saving leaves it on hold.
    QPushButton *orderHistory = button(cashier, QStringLiteral("Order History"));
    check(orderHistory, QStringLiteral("Cashier has no Order History button."));
    if (!orderHistory)
        return;
    orderHistory->click();
    settle();
    QTableWidget *heldHistory = nullptr;
    for (QTableWidget *table : cashier->findChildren<QTableWidget *>())
        if (table->isVisible())
            heldHistory = table;
    int heldRow = -1;
    for (int row = 0; heldHistory && row < heldHistory->rowCount(); ++row)
        if (heldHistory->item(row, 0) && heldHistory->item(row, 0)->text() == heldOrderId)
            heldRow = row;
    check(heldHistory && heldRow >= 0,
        QStringLiteral("Order History does not show held order %1.").arg(heldOrderId));
    if (heldHistory && heldRow >= 0)
    {
        heldHistory->selectRow(heldRow);
        QPushButton *edit = button(cashier, QStringLiteral("Edit order"));
        check(edit && edit->isEnabled(), QStringLiteral("On-hold order should be editable."));
        if (edit)
        {
            edit->click();
            settle();
            check(hasLabel(cashier, QStringLiteral("New Order"))
                    && button(cashier, QStringLiteral("Save changes")),
                QStringLiteral("Editing an on-hold order should open the edit form directly."));
            if (QLineEdit *editedCustomer = field(cashier, QStringLiteral("Enter customer name")))
                editedCustomer->setText(QStringLiteral("CI Edited"));
            if (QPushButton *save = button(cashier, QStringLiteral("Save changes")))
                save->click();
            settle();
            QVector<Pos::Order> afterHeldEdit;
            check(Pos::readOrders(Pos::DataPaths::orders(), afterHeldEdit, error)
                    && afterHeldEdit.last().status == QStringLiteral("on-hold")
                    && afterHeldEdit.last().customerName == QStringLiteral("CI_Edited"),
                QStringLiteral("Saving an on-hold edit should preserve its status and customer details (%1).")
                    .arg(error));
            check(stockOf(QStringLiteral("P0001")) == stockBefore,
                QStringLiteral("Editing an on-hold order must not change stock."));
        }
    }

    // A newly created dashboard after sign-out must reload the held order from disk.
    cashier = openDashboard(stack, QStringLiteral("Cashier"));
    QPushButton *resumeQuickAction = nullptr;
    for (QPushButton *candidate : cashier->findChildren<QPushButton *>())
        if (candidate->isVisible()
            && candidate->accessibleName() == QStringLiteral("Resume latest order"))
            resumeQuickAction = candidate;
    check(resumeQuickAction,
        QStringLiteral("Dashboard should offer direct resume when an order is held."));
    if (resumeQuickAction)
    {
        resumeQuickAction->click();
        settle();
        check(hasLabel(cashier, QStringLiteral("New Order")),
            QStringLiteral("The dashboard Resume action should open New Order directly."));
        QPushButton *dashboard = button(cashier, QStringLiteral("Dashboard"));
        check(dashboard, QStringLiteral("Cashier: no Dashboard navigation button."));
        if (dashboard)
        {
            dashboard->click();
            settle();
        }
    }
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
    QVector<Pos::Product> products;
    Pos::Product milk;
    Pos::CartTotals expectedTotals;
    QComboBox *specialDiscount = nullptr;
    for (QComboBox *candidate : cashier->findChildren<QComboBox *>())
        if (candidate->isVisible()
            && candidate->findText(QStringLiteral("8%")) >= 0)
            specialDiscount = candidate;
    check(specialDiscount,
        QStringLiteral("New Order should offer a special discount up to 8%."));
    if (specialDiscount)
        specialDiscount->setCurrentText(QStringLiteral("8%"));
    check(Pos::readProducts(Pos::DataPaths::products(), products, error)
            && Pos::findProductByBarcode(products, QStringLiteral("100001"), milk)
            && Pos::calculateCartTotals({Pos::CartItem{milk, 1}}, expectedTotals, error, 8)
            && expectedTotals.seasonalDiscount > 0,
        QStringLiteral("Seasonal and special discounts should be included in the order total (%1).")
            .arg(error));
    check(showsText(cashier, QStringLiteral("Seasonal discount")),
        QStringLiteral("New Order should show the seasonal discount in its summary."));
    check(showsText(cashier, QStringLiteral("Special discount")),
        QStringLiteral("New Order should show the special discount in its summary."));
    QDoubleSpinBox *cashAmount = nullptr;
    for (QDoubleSpinBox *candidate : cashier->findChildren<QDoubleSpinBox *>())
        if (candidate->isVisible())
            cashAmount = candidate;
    check(cashAmount && qAbs(cashAmount->value() - expectedTotals.payable) < 0.005,
        QStringLiteral("Cash received should default to the discounted total."));

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
    check(cashierBill.seasonalDiscount > 0 && cashierBill.specialDiscount > 0
            && qAbs(cashierBill.total - expectedTotals.payable) < 0.005,
        QStringLiteral("The saved bill should include both discounts in its final total."));
    QString checkoutError;
    for (QLabel *label : cashier->findChildren<QLabel *>())
        if (label->isVisible() && (label->text().startsWith(QStringLiteral("Bill not saved:"))
                || label->text().startsWith(QStringLiteral("Add at least one product"))))
            checkoutError = label->text();
    check(sale.customerName == QStringLiteral("CI_Edited") && sale.status == QStringLiteral("paid") &&
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

    orderHistory = button(cashier, QStringLiteral("Order History"));
    check(orderHistory, QStringLiteral("Cashier: no Order History button."));
    if (!orderHistory)
        return;
    orderHistory->click();
    settle();
    check(showsText(cashier, sale.id),
        QStringLiteral("Order History does not show the new order %1.").arg(sale.id));
    QTableWidget *saleHistory = nullptr;
    int saleRow = -1;
    for (QTableWidget *table : cashier->findChildren<QTableWidget *>())
    {
        if (!table->isVisible())
            continue;
        for (int row = 0; row < table->rowCount(); ++row)
            if (table->item(row, 0) && table->item(row, 0)->text() == sale.id)
            {
                saleHistory = table;
                saleRow = row;
                break;
            }
    }
    check(saleHistory && saleRow >= 0,
        QStringLiteral("Could not select the new paid order for editing."));
    if (saleHistory && saleRow >= 0)
    {
        saleHistory->selectRow(saleRow);
        QPushButton *edit = button(cashier, QStringLiteral("Edit order"));
        check(edit && edit->isEnabled(), QStringLiteral("Paid order should offer an edit action."));
        if (edit)
        {
            approveManagerEdit();
            edit->click();
            settle();
            popupHandler = nullptr;
            check(hasLabel(cashier, QStringLiteral("New Order"))
                    && button(cashier, QStringLiteral("Save changes"))
                    && !button(cashier, QStringLiteral("Generate bill")),
                QStringLiteral("Manager-approved paid edit should open an edit-only form."));
            QTableWidget *cart = nullptr;
            for (QTableWidget *table : cashier->findChildren<QTableWidget *>())
                if (table->isVisible() && table->columnCount() == 5)
                    cart = table;
            QWidget *quantityControl = cart ? cart->cellWidget(0, 3) : nullptr;
            QPushButton *increase = quantityControl
                ? button(quantityControl, QStringLiteral("+")) : nullptr;
            check(increase && increase->isEnabled(),
                QStringLiteral("Paid edit should allow increasing an item using reserved stock."));
            if (increase)
                increase->click();
            if (QPushButton *save = button(cashier, QStringLiteral("Save changes")))
                save->click();
            settle();
            QVector<Pos::Order> afterPaidEdit;
            QVector<Pos::OrderDetail> editedDetails;
            check(Pos::readOrders(Pos::DataPaths::orders(), afterPaidEdit, error)
                    && afterPaidEdit.last().id == sale.id
                    && afterPaidEdit.last().status == QStringLiteral("paid")
                    && Pos::readOrderDetails(Pos::DataPaths::orderDetail(), sale.id, editedDetails, error)
                    && editedDetails.size() == 1 && editedDetails.first().quantity == 2,
                QStringLiteral("Paid edit should save the same paid order with updated details (%1).")
                    .arg(error));
            check(stockOf(QStringLiteral("P0001")) == stockBefore - 2,
                QStringLiteral("Paid edit must apply only the stock difference."));
        }
    }

    QWidget *manager = openDashboard(stack, QStringLiteral("Manager"));
    QPushButton *managerHistory = button(manager, QStringLiteral("Order History"));
    check(managerHistory, QStringLiteral("Manager: no Order History button."));
    if (!managerHistory)
        return;
    managerHistory->click();
    settle();
    QTableWidget *historyTable = nullptr;
    int managerSaleRow = -1;
    for (QTableWidget *table : manager->findChildren<QTableWidget *>())
    {
        if (!table->isVisible())
            continue;
        for (int row = 0; row < table->rowCount(); ++row)
            if (table->item(row, 0) && table->item(row, 0)->text() == sale.id)
            {
                historyTable = table;
                managerSaleRow = row;
                break;
            }
    }
    check(historyTable && managerSaleRow >= 0,
        QStringLiteral("Manager Order History does not list the paid order."));
    if (!historyTable || managerSaleRow < 0)
        return;
    historyTable->item(managerSaleRow, 0)->setSelected(true);
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

    // Req 3: a signed-in manager edits a paid order with no approval form
    // (the approval is for cashiers), gets the edit form (not Dashboard),
    // and returns to Order History after saving.
    historyTable->clearSelection();
    historyTable->item(managerSaleRow, 0)->setSelected(true);
    QPushButton *managerEdit = button(manager, QStringLiteral("Edit order"));
    check(managerEdit && managerEdit->isEnabled(),
        QStringLiteral("Manager: a recent paid order should offer Edit order."));
    if (!managerEdit || !managerEdit->isEnabled())
        return;
    bool approvalAsked = false;
    popupHandler = [&approvalAsked](QWidget *popup) {
        if (popup->objectName() != QStringLiteral("managerApprovalDialog"))
            return false;
        approvalAsked = true;
        if (QPushButton *cancel = button(popup, QStringLiteral("Cancel")))
            cancel->click();
        return true;
    };
    managerEdit->click();
    settle();
    popupHandler = nullptr;
    check(!approvalAsked,
        QStringLiteral("A signed-in manager must not be asked for manager approval."));
    check(button(manager, QStringLiteral("Save changes")) && !hasLabel(manager, QStringLiteral("Order History")),
        QStringLiteral("Manager edit must open the edit form, not fall back to another page."));
    if (QPushButton *save = button(manager, QStringLiteral("Save changes")))
        save->click();
    settle();
    check(hasLabel(manager, QStringLiteral("Order History")),
        QStringLiteral("After saving an edit, the manager must be back on Order History."));
}
}

int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    qunsetenv("DUKAANOS_THEME");

    QApplication app(argc, argv);
    UiTheme::install(app);
    check(!UiTheme::isDark(), QStringLiteral("The app must start in light mode by default."));

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
    testCategoriesPage(stack);
    testFormsInDarkMode();
    testReportsPage(stack);
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
