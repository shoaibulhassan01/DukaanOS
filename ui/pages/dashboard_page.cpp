#include "dashboard_page.h"
#include "bill_page.h"
#include "held_orders_page.h"
#include "new_order_page.h"
#include "order_history_page.h"
#include "products_page.h"
#include "scan_product_page.h"

#include <QComboBox>
#include <QDateTime>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPolygonF>
#include <QPushButton>
#include <QKeySequence>
#include <QShortcut>
#include <QMessageBox>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

namespace
{
const QString kNavy = QStringLiteral("#111184");
const QString kInk = QStringLiteral("#20233B");
const QString kMuted = QStringLiteral("#777B91");
const QString kLine = QStringLiteral("#E7E9F3");
const QString kPage = QStringLiteral("#F4F5FB");

struct NavItem
{
    QString title;
    QString icon;        // key used by drawIcon()
    QString description;
    QString accent;      // icon / hover colour on the dashboard cards
    QString tint;        // soft background behind the icon
};

// ---------------------------------------------------------------------------
// Small vector icon set drawn in code (no image assets needed).
// All icons are designed on a 24x24 grid and rendered crisp on HiDPI screens.
// ---------------------------------------------------------------------------
QPixmap drawIcon(const QString &name, const QColor &color, int size)
{
    const qreal dpr = 2.0;
    QPixmap pm(int(size * dpr), int(size * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(size / 24.0, size / 24.0);
    p.setPen(QPen(color, 1.9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    auto poly = [&p](std::initializer_list<QPointF> pts, bool close = false) {
        QPolygonF g;
        for (const QPointF &pt : pts)
            g << pt;
        if (close)
            p.drawPolygon(g);
        else
            p.drawPolyline(g);
    };

    if (name == QStringLiteral("dashboard"))
    {
        p.drawRoundedRect(QRectF(3, 3, 7, 7), 2, 2);
        p.drawRoundedRect(QRectF(14, 3, 7, 7), 2, 2);
        p.drawRoundedRect(QRectF(3, 14, 7, 7), 2, 2);
        p.drawRoundedRect(QRectF(14, 14, 7, 7), 2, 2);
    }
    else if (name == QStringLiteral("cart"))
    {
        poly({QPointF(2, 3), QPointF(5, 3), QPointF(7, 15), QPointF(19, 15), QPointF(21, 7), QPointF(6, 7)});
        p.drawEllipse(QPointF(9, 20), 1.6, 1.6);
        p.drawEllipse(QPointF(17, 20), 1.6, 1.6);
    }
    else if (name == QStringLiteral("history"))
    {
        p.drawEllipse(QPointF(12, 12), 9, 9);
        poly({QPointF(12, 7), QPointF(12, 12), QPointF(15.5, 14)});
    }
    else if (name == QStringLiteral("hold"))
    {
        p.drawRoundedRect(QRectF(3, 3, 18, 18), 4, 4);
        p.drawLine(QPointF(9, 8), QPointF(9, 16));
        p.drawLine(QPointF(15, 8), QPointF(15, 16));
    }
    else if (name == QStringLiteral("scan"))
    {
        poly({QPointF(3, 8), QPointF(3, 3), QPointF(8, 3)});
        poly({QPointF(16, 3), QPointF(21, 3), QPointF(21, 8)});
        poly({QPointF(21, 16), QPointF(21, 21), QPointF(16, 21)});
        poly({QPointF(8, 21), QPointF(3, 21), QPointF(3, 16)});
        p.drawLine(QPointF(3, 12), QPointF(21, 12));
    }
    else if (name == QStringLiteral("box"))
    {
        poly({QPointF(12, 3), QPointF(20, 7.5), QPointF(20, 16.5),
              QPointF(12, 21), QPointF(4, 16.5), QPointF(4, 7.5)}, true);
        poly({QPointF(4, 7.5), QPointF(12, 12), QPointF(20, 7.5)});
        p.drawLine(QPointF(12, 12), QPointF(12, 21));
    }
    else if (name == QStringLiteral("receipt"))
    {
        p.drawRoundedRect(QRectF(5, 3, 14, 18), 2.5, 2.5);
        p.drawLine(QPointF(8.5, 8), QPointF(15.5, 8));
        p.drawLine(QPointF(8.5, 12), QPointF(15.5, 12));
        p.drawLine(QPointF(8.5, 16), QPointF(12.5, 16));
    }
    else if (name == QStringLiteral("trend"))
    {
        poly({QPointF(3, 17), QPointF(9, 11), QPointF(13, 15), QPointF(21, 6)});
        poly({QPointF(16, 6), QPointF(21, 6), QPointF(21, 11)});
    }
    else if (name == QStringLiteral("user"))
    {
        p.drawEllipse(QPointF(12, 8), 4, 4);
        p.drawArc(QRectF(4, 14, 16, 14), 0, 180 * 16);
    }
    else if (name == QStringLiteral("logout"))
    {
        poly({QPointF(10, 3), QPointF(4, 3), QPointF(4, 21), QPointF(10, 21)});
        p.drawLine(QPointF(10, 12), QPointF(21, 12));
        poly({QPointF(17, 8), QPointF(21, 12), QPointF(17, 16)});
    }
    else if (name == QStringLiteral("chevron"))
    {
        poly({QPointF(9, 5), QPointF(16, 12), QPointF(9, 19)});
    }
    p.end();
    return pm;
}

class DashboardWindow : public QWidget
{
public:
    explicit DashboardWindow(QWidget *parent, const QString &initialRole) : QWidget(parent)
    {
        setObjectName(QStringLiteral("dashboardRoot"));
        setAttribute(Qt::WA_StyledBackground, true);
        setWindowTitle(QStringLiteral("DukaanOS | Dashboard"));
        setWindowIcon(QIcon(QStringLiteral(":/assets/app_icon.png")));
        setMinimumSize(1080, 700);
        resize(1360, 840);
        setFont(QFont(QStringLiteral("Segoe UI"), 10));
        setStyleSheet(QStringLiteral("#dashboardRoot { background: %1; }").arg(kPage));

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);
        root->addWidget(createHeader());

        auto *body = new QHBoxLayout;
        body->setContentsMargins(0, 0, 0, 0);
        body->setSpacing(0);
        body->addWidget(createSidebar());

        auto *mainArea = new QWidget(this);
        auto *mainLayout = new QVBoxLayout(mainArea);
        mainLayout->setContentsMargins(36, 28, 36, 28);
        mainLayout->setSpacing(0);

        headerTitle_ = new QLabel(mainArea);
        headerTitle_->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 24px; font-weight: 700; background: transparent;").arg(kInk));
        headerSubtitle_ = new QLabel(mainArea);
        headerSubtitle_->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 12px; background: transparent;").arg(kMuted));
        // Owner: Umar Khan - Refresh button (and F5) rebuilds the current page,
        // so it re-reads its .txt files and resets its state.
        auto *titleRow = new QHBoxLayout;
        titleRow->setContentsMargins(0, 0, 0, 0);
        titleRow->addWidget(headerTitle_);
        titleRow->addStretch(1);
        auto *refresh = new QPushButton(QStringLiteral("Refresh"), mainArea);
        refresh->setCursor(Qt::PointingHandCursor);
        refresh->setFixedHeight(34);
        refresh->setToolTip(QStringLiteral("Reload this page (F5)"));
        refresh->setStyleSheet(QStringLiteral(
            "QPushButton { background: white; color: %1; border: 1px solid %2;"
            " border-radius: 8px; padding: 0 16px; font-size: 11px; font-weight: 600; }"
            "QPushButton:hover { background: #F0F0FF; border-color: #E1E2FA; }").arg(kNavy, kLine));
        titleRow->addWidget(refresh);
        connect(refresh, &QPushButton::clicked, this, [this]() { refreshPage(); });
        auto *refreshKey = new QShortcut(QKeySequence::Refresh, this);
        connect(refreshKey, &QShortcut::activated, this, [this]() { refreshPage(); });
        mainLayout->addLayout(titleRow);
        mainLayout->addSpacing(4);
        mainLayout->addWidget(headerSubtitle_);
        mainLayout->addSpacing(22);

        auto *contentScroll = new QScrollArea(mainArea);
        contentScroll->setWidgetResizable(true);
        contentScroll->setFrameShape(QFrame::NoFrame);
        contentScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        contentScroll->setStyleSheet(QStringLiteral(
            "QScrollArea { background: transparent; border: none; }"));

        contentHost_ = new QWidget;
        contentLayout_ = new QVBoxLayout(contentHost_);
        contentLayout_->setContentsMargins(0, 0, 0, 0);
        contentLayout_->setSizeConstraint(QLayout::SetMinimumSize);
        contentScroll->setWidget(contentHost_);
        mainLayout->addWidget(contentScroll, 1);

        body->addWidget(mainArea, 1);
        root->addLayout(body, 1);

        int initialRoleIndex = -1;
        for (int i = 0; i < roleCombo_->count(); ++i)
        {
            if (roleCombo_->itemText(i).compare(initialRole, Qt::CaseInsensitive) == 0)
            {
                initialRoleIndex = i;
                break;
            }
        }
        if (initialRoleIndex >= 0)
            roleCombo_->setCurrentIndex(initialRoleIndex);
        updateRole();
    }

private:
    // ---------------------------------------------------------------- header
    QWidget *createHeader()
    {
        auto *header = new QFrame(this);
        header->setObjectName(QStringLiteral("topHeader"));
        header->setFixedHeight(72);
        header->setStyleSheet(QStringLiteral(
            "QFrame#topHeader { background: %1; border-bottom: 1px solid rgba(255,255,255,28); }")
            .arg(kNavy));

        auto *layout = new QHBoxLayout(header);
        layout->setContentsMargins(24, 0, 28, 0);
        layout->setSpacing(0);

        // Use the uploaded brand colours in the header.
        auto *logo = new QLabel(header);
        logo->setStyleSheet(QStringLiteral("background: transparent; color: white;"
                                           " font-size: 20px; font-weight: 800;"));
        const QPixmap wordmark(QStringLiteral(":/assets/dukaanos_wordmark.png"));
        if (!wordmark.isNull())
            logo->setPixmap(wordmark.scaledToHeight(40, Qt::SmoothTransformation));
        else
            logo->setText(QStringLiteral("DukaanOS"));
        layout->addWidget(logo);
        layout->addStretch(1);

        // Role (right)
        auto *roleIcon = new QLabel(header);
        roleIcon->setFixedSize(22, 22);
        roleIcon->setPixmap(drawIcon(QStringLiteral("user"), Qt::white, 20));
        roleIcon->setStyleSheet(QStringLiteral("background: transparent;"));
        layout->addWidget(roleIcon);
        layout->addSpacing(8);

        auto *roleLabel = new QLabel(QStringLiteral("Role"), header);
        roleLabel->setStyleSheet(QStringLiteral(
            "color: rgba(255,255,255,170); font-size: 12px; font-weight: 600;"
            " background: transparent;"));
        layout->addWidget(roleLabel);
        layout->addSpacing(10);

        roleCombo_ = new QComboBox(header);
        roleCombo_->addItems({QStringLiteral("Cashier"), QStringLiteral("Manager")});
        roleCombo_->setFixedSize(140, 40);
        roleCombo_->setCursor(Qt::PointingHandCursor);
        roleCombo_->setStyleSheet(QStringLiteral(
            "QComboBox { background: rgba(255,255,255,30); color: white;"
            " border: 1px solid rgba(255,255,255,60); border-radius: 10px;"
            " padding: 0 12px; font-size: 12px; font-weight: 600; }"
            "QComboBox:hover { background: rgba(255,255,255,46); }"
            "QComboBox::drop-down { border: none; width: 26px; }"
            "QComboBox QAbstractItemView { background: white; color: %1;"
            " selection-background-color: #EEF0FF; selection-color: %2;"
            " border: 1px solid %3; outline: 0; }")
            .arg(kInk, kNavy, kLine));
        layout->addWidget(roleCombo_);
        layout->addSpacing(16);

        // Logout
        auto *logout = new QPushButton(QStringLiteral("  Logout"), header);
        logout->setIcon(QIcon(drawIcon(QStringLiteral("logout"), Qt::white, 18)));
        logout->setIconSize(QSize(18, 18));
        logout->setCursor(Qt::PointingHandCursor);
        logout->setFixedSize(112, 40);
        logout->setStyleSheet(QStringLiteral(
            "QPushButton { background: transparent; color: white;"
            " border: 1px solid rgba(255,255,255,90); border-radius: 10px;"
            " font-size: 12px; font-weight: 600; }"
            "QPushButton:hover { background: rgba(255,255,255,36); }"
            "QPushButton:pressed { background: rgba(255,255,255,56); }"));
        // Owner: Shoaib Ul Hassan - return to the login page on sign-out.
        connect(logout, &QPushButton::clicked, this, [this]() {
            if (auto *stack = qobject_cast<QStackedWidget *>(parentWidget()))
                stack->setCurrentIndex(0);
            else
                close();
        });
        layout->addWidget(logout);

        connect(roleCombo_, &QComboBox::currentTextChanged,
            this, [this]() { updateRole(); });
        return header;
    }

    // --------------------------------------------------------------- sidebar
    QWidget *createSidebar()
    {
        auto *sidebar = new QFrame(this);
        sidebar->setObjectName(QStringLiteral("sideBar"));
        sidebar->setFixedWidth(248);
        sidebar->setStyleSheet(QStringLiteral(
            "QFrame#sideBar { background: %1; }").arg(kNavy));
        auto *layout = new QVBoxLayout(sidebar);
        layout->setContentsMargins(16, 26, 16, 20);
        layout->setSpacing(0);

        auto *section = new QLabel(QStringLiteral("WORKSPACE"), sidebar);
        section->setStyleSheet(QStringLiteral(
            "color: rgba(255,255,255,120); font-size: 10px; font-weight: 700;"
            " letter-spacing: 1px; background: transparent; padding-left: 6px;"));
        layout->addWidget(section);
        layout->addSpacing(12);

        navigationLayout_ = new QVBoxLayout;
        navigationLayout_->setSpacing(6);
        layout->addLayout(navigationLayout_);
        layout->addStretch(1);

        auto *footer = new QLabel(QStringLiteral("DukaanOS  |  POS workspace"), sidebar);
        footer->setStyleSheet(QStringLiteral(
            "color: rgba(255,255,255,100); font-size: 10px; background: transparent;"
            " padding-left: 6px;"));
        layout->addWidget(footer);
        return sidebar;
    }

    QVector<NavItem> itemsForRole() const
    {
        const NavItem dashboard{QStringLiteral("Dashboard"), QStringLiteral("dashboard"),
            QStringLiteral("Store overview"), QStringLiteral("#111184"), QStringLiteral("#ECEEFF")};
        const NavItem history{QStringLiteral("Order History"), QStringLiteral("history"),
            QStringLiteral("Review orders"), QStringLiteral("#7C4DFF"), QStringLiteral("#F0EAFF")};
        const NavItem heldOrders{QStringLiteral("Held Orders"), QStringLiteral("hold"),
            QStringLiteral("Resume a saved order"), QStringLiteral("#0EA5E9"), QStringLiteral("#E0F4FE")};

        if (roleCombo_->currentText() == QStringLiteral("Manager"))
        {
            return {
                dashboard,
                {QStringLiteral("Scan Product"), QStringLiteral("scan"),
                    QStringLiteral("Look up an item"), QStringLiteral("#0EA5E9"), QStringLiteral("#E0F4FE")},
                {QStringLiteral("Products"), QStringLiteral("box"),
                    QStringLiteral("Manage inventory"), QStringLiteral("#F59E0B"), QStringLiteral("#FEF3DC")},
                history
            };
        }
        return {
            dashboard,
            {QStringLiteral("New Order"), QStringLiteral("cart"),
                QStringLiteral("Start a sale"), QStringLiteral("#F97316"), QStringLiteral("#FFEDE0")},
            {QStringLiteral("Scan Product"), QStringLiteral("scan"),
                QStringLiteral("Look up an item"), QStringLiteral("#0EA5E9"), QStringLiteral("#E0F4FE")},
            heldOrders,
            history
        };
    }

    void refreshPage()
    {
        // Rebuilding New Order would throw away the items in the cart.
        if (currentPage_ == QStringLiteral("New Order") &&
            QMessageBox::question(this, QStringLiteral("Refresh New Order"),
                QStringLiteral("Refreshing clears the current order. Continue?"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
        navigateTo(currentPage_);
    }

    void navigateTo(const QString &page)
    {
        currentPage_ = page;
        updateRole();
    }

    void updateRole()
    {
        const QVector<NavItem> items = itemsForRole();
        bool pageAvailable = currentPage_ == QStringLiteral("Bill");
        for (const NavItem &item : items)
            pageAvailable = pageAvailable || item.title == currentPage_;
        if (!pageAvailable)
            currentPage_ = QStringLiteral("Dashboard");

        while (QLayoutItem *item = navigationLayout_->takeAt(0))
        {
            if (QWidget *widget = item->widget())
                widget->deleteLater();
            delete item;
        }

        for (const NavItem &item : items)
        {
            auto *button = new QPushButton(QStringLiteral("   ") + item.title, this);
            button->setIcon(QIcon(drawIcon(item.icon, Qt::white, 20)));
            button->setIconSize(QSize(20, 20));
            button->setCursor(Qt::PointingHandCursor);
            button->setFixedHeight(46);
            const bool active = currentPage_ == item.title;
            button->setStyleSheet(QStringLiteral(
                "QPushButton { text-align: left; padding-left: 16px; border: none;"
                " border-radius: 11px; color: white; background: %1;"
                " font-size: 13px; font-weight: %2; }"
                "QPushButton:hover { background: %3; }")
                .arg(active ? QStringLiteral("rgba(255,255,255,46)") : QStringLiteral("transparent"),
                     active ? QStringLiteral("700") : QStringLiteral("500"),
                     active ? QStringLiteral("rgba(255,255,255,56)") : QStringLiteral("rgba(255,255,255,24)")));
            navigationLayout_->addWidget(button);
            connect(button, &QPushButton::clicked, this, [this, item]() { navigateTo(item.title); });
        }
        navigationLayout_->addStretch(1);

        const QString role = roleCombo_->currentText();
        headerTitle_->setText(currentPage_);
        headerSubtitle_->setText(currentPage_ == QStringLiteral("Dashboard")
            ? QStringLiteral("Here is your store at a glance.")
            : QStringLiteral("%1 preview  •  %2").arg(role, currentPage_));

        clearContent();
        if (currentPage_ == QStringLiteral("Dashboard"))
            showDashboard(items);
        else if (currentPage_ == QStringLiteral("New Order"))
        {
            const OrderPreview *initialOrder = hasResumeOrder_ ? &resumeOrder_ : nullptr;
            contentLayout_->addWidget(UiPages::createNewOrderPage(contentHost_, initialOrder,
                [this](const OrderPreview &order) {
                    // The bill is already saved; show it in a pop-up, then
                    // start a fresh order when it is closed.
                    UiPages::showBillDialog(order, this);
                    navigateTo(QStringLiteral("New Order"));
                },
                [this](const OrderPreview &order) {
                    OrderPreview heldOrder = order;
                    heldOrder.id = QStringLiteral("HOLD-%1")
                        .arg(++heldOrderCounter_, 4, 10, QLatin1Char('0'));
                    heldOrder.createdAt = QDateTime::currentDateTime()
                        .toString(QStringLiteral("dd MMM, hh:mm AP"));
                    heldOrders_.append(heldOrder);
                    hasResumeOrder_ = false;
                    navigateTo(QStringLiteral("Held Orders"));
                }));
            hasResumeOrder_ = false;
        }
        else if (currentPage_ == QStringLiteral("Held Orders"))
            showHeldOrders();
        else if (currentPage_ == QStringLiteral("Order History"))
            contentLayout_->addWidget(UiPages::createOrderHistoryPage(contentHost_,
                [this]() { navigateTo(QStringLiteral("New Order")); }));
        else if (currentPage_ == QStringLiteral("Scan Product"))
            contentLayout_->addWidget(UiPages::createScanProductPage(contentHost_));
        else if (currentPage_ == QStringLiteral("Products"))
            contentLayout_->addWidget(UiPages::createProductsPage(contentHost_));
        else if (currentPage_ == QStringLiteral("Bill") && hasBillPreview_)
            contentLayout_->addWidget(UiPages::createBillPage(billPreview_, contentHost_,
                [this]() { navigateTo(QStringLiteral("New Order")); }));
        else
            showPlaceholder(currentPage_);
    }

    void clearContent()
    {
        while (QLayoutItem *item = contentLayout_->takeAt(0))
        {
            if (QWidget *widget = item->widget())
                widget->deleteLater();
            delete item;
        }
    }

    // ------------------------------------------------------------ components
    QFrame *createBanner(QWidget *parent, const NavItem &primary)
    {
        auto *banner = new QFrame(parent);
        banner->setObjectName(QStringLiteral("banner"));
        banner->setMinimumHeight(118);
        banner->setStyleSheet(QStringLiteral(
            "QFrame#banner { border-radius: 18px; background: qlineargradient("
            "x1:0, y1:0, x2:1, y2:1, stop:0 #111184, stop:1 #5B4BFF); }"));
        auto *layout = new QHBoxLayout(banner);
        layout->setContentsMargins(30, 22, 30, 22);

        auto *texts = new QVBoxLayout;
        texts->setSpacing(6);
        auto *title = new QLabel(QStringLiteral("Ready for the next customer?"), banner);
        title->setStyleSheet(QStringLiteral(
            "color: white; font-size: 21px; font-weight: 700; background: transparent;"));
        auto *sub = new QLabel(QStringLiteral("Signed in as %1. Jump straight into your most-used tool.")
                                   .arg(roleCombo_->currentText()), banner);
        sub->setStyleSheet(QStringLiteral(
            "color: rgba(255,255,255,190); font-size: 12px; background: transparent;"));
        texts->addStretch(1);
        texts->addWidget(title);
        texts->addWidget(sub);
        texts->addStretch(1);
        layout->addLayout(texts);
        layout->addStretch(1);

        auto *cta = new QPushButton(QStringLiteral("  ") + primary.title, banner);
        cta->setIcon(QIcon(drawIcon(primary.icon, QColor(kNavy), 20)));
        cta->setIconSize(QSize(20, 20));
        cta->setCursor(Qt::PointingHandCursor);
        cta->setFixedSize(170, 46);
        cta->setStyleSheet(QStringLiteral(
            "QPushButton { background: white; color: %1; border: none; border-radius: 12px;"
            " font-size: 13px; font-weight: 700; }"
            "QPushButton:hover { background: #EEF0FF; }").arg(kNavy));
        connect(cta, &QPushButton::clicked, this, [this, primary]() { navigateTo(primary.title); });
        layout->addWidget(cta);
        return banner;
    }

    QFrame *createStatCard(QWidget *parent, const QString &label, const QString &value,
        const QString &caption, const QString &iconName, const QString &accent, const QString &tint)
    {
        auto *card = new QFrame(parent);
        card->setObjectName(QStringLiteral("statCard"));
        card->setStyleSheet(QStringLiteral(
            "QFrame#statCard { background: white; border: 1px solid %1; border-radius: 16px; }")
            .arg(kLine));
        auto *layout = new QHBoxLayout(card);
        layout->setContentsMargins(20, 18, 20, 18);
        layout->setSpacing(16);

        auto *tile = new QLabel(card);
        tile->setFixedSize(54, 54);
        tile->setAlignment(Qt::AlignCenter);
        tile->setPixmap(drawIcon(iconName, QColor(accent), 26));
        tile->setStyleSheet(QStringLiteral("background: %1; border-radius: 15px;").arg(tint));
        layout->addWidget(tile);

        auto *texts = new QVBoxLayout;
        texts->setSpacing(3);
        auto *name = new QLabel(label, card);
        name->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 12px; font-weight: 600; background: transparent;").arg(kMuted));
        auto *number = new QLabel(value, card);
        number->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 26px; font-weight: 700; background: transparent;").arg(kInk));
        auto *hint = new QLabel(caption, card);
        hint->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 11px; font-weight: 600; background: transparent;").arg(accent));
        texts->addWidget(name);
        texts->addWidget(number);
        texts->addWidget(hint);
        layout->addLayout(texts, 1);
        return card;
    }

    QPushButton *createActionCard(QWidget *parent, const NavItem &item)
    {
        auto *button = new QPushButton(parent);
        button->setObjectName(QStringLiteral("actionCard"));
        button->setCursor(Qt::PointingHandCursor);
        button->setMinimumHeight(92);
        button->setStyleSheet(QStringLiteral(
            "QPushButton#actionCard { background: white; border: 1px solid %1; border-radius: 16px; }"
            "QPushButton#actionCard:hover { background: #FAFAFF; border: 1px solid %2; }")
            .arg(kLine, item.accent));

        auto *layout = new QHBoxLayout(button);
        layout->setContentsMargins(18, 14, 18, 14);
        layout->setSpacing(16);

        auto *tile = new QLabel(button);
        tile->setFixedSize(52, 52);
        tile->setAlignment(Qt::AlignCenter);
        tile->setPixmap(drawIcon(item.icon, QColor(item.accent), 26));
        tile->setStyleSheet(QStringLiteral("background: %1; border-radius: 15px;").arg(item.tint));
        layout->addWidget(tile);

        auto *texts = new QVBoxLayout;
        texts->setSpacing(3);
        auto *title = new QLabel(item.title, button);
        title->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 14px; font-weight: 700; background: transparent;").arg(kInk));
        auto *desc = new QLabel(item.description, button);
        desc->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 11px; background: transparent;").arg(kMuted));
        texts->addStretch(1);
        texts->addWidget(title);
        texts->addWidget(desc);
        texts->addStretch(1);
        layout->addLayout(texts, 1);

        auto *chevron = new QLabel(button);
        chevron->setPixmap(drawIcon(QStringLiteral("chevron"), QColor(kMuted), 18));
        chevron->setStyleSheet(QStringLiteral("background: transparent;"));
        layout->addWidget(chevron);

        // Let the whole card receive the click.
        for (QWidget *child : {static_cast<QWidget *>(tile), static_cast<QWidget *>(title),
                 static_cast<QWidget *>(desc), static_cast<QWidget *>(chevron)})
            child->setAttribute(Qt::WA_TransparentForMouseEvents, true);

        connect(button, &QPushButton::clicked, this, [this, item]() { navigateTo(item.title); });
        return button;
    }

    // ----------------------------------------------------------------- pages
    void showDashboard(const QVector<NavItem> &items)
    {
        auto *page = new QWidget(contentHost_);
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 64);
        layout->setSpacing(0);

        if (items.size() > 1)
        {
            layout->addWidget(createBanner(page, items.at(1)));
            layout->addSpacing(26);
        }

        auto *overview = new QLabel(QStringLiteral("TODAY'S OVERVIEW"), page);
        overview->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; font-weight: 700; letter-spacing: 1px;"
            " background: transparent;").arg(kMuted));
        layout->addWidget(overview);
        layout->addSpacing(12);

        auto *stats = new QGridLayout;
        stats->setHorizontalSpacing(16);
        stats->setVerticalSpacing(14);
        stats->setColumnStretch(0, 1);
        stats->setColumnStretch(1, 1);
        stats->addWidget(createStatCard(page, QStringLiteral("Total Orders"), QStringLiteral("0"),
            QStringLiteral("No orders yet"), QStringLiteral("receipt"),
            QStringLiteral("#4F46E5"), QStringLiteral("#EEEFFF")), 0, 0);
        stats->addWidget(createStatCard(page, QStringLiteral("Total Sales"), QStringLiteral("PKR 0"),
            QStringLiteral("No sales yet"), QStringLiteral("trend"),
            QStringLiteral("#0E9F6E"), QStringLiteral("#E1F7EE")), 0, 1);
        layout->addLayout(stats);
        layout->addSpacing(28);

        auto *actionsHeader = new QHBoxLayout;
        auto *actionsTitle = new QLabel(QStringLiteral("Quick actions"), page);
        actionsTitle->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 17px; font-weight: 700; background: transparent;").arg(kInk));
        actionsHeader->addWidget(actionsTitle);
        actionsHeader->addStretch(1);
        auto *previewLabel = new QLabel(QStringLiteral("Choose a section"), page);
        previewLabel->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 11px; background: transparent;").arg(kMuted));
        actionsHeader->addWidget(previewLabel);
        layout->addLayout(actionsHeader);
        layout->addSpacing(14);

        auto *actions = new QGridLayout;
        actions->setHorizontalSpacing(16);
        actions->setVerticalSpacing(14);
        actions->setColumnStretch(0, 1);
        actions->setColumnStretch(1, 1);
        int column = 0;
        int row = 0;
        for (const NavItem &item : items)
        {
            if (item.title == QStringLiteral("Dashboard"))
                continue;
            actions->addWidget(createActionCard(page, item), row, column);
            if (++column == 2)
            {
                column = 0;
                ++row;
            }
        }
        layout->addLayout(actions);
        layout->addStretch(1);

        contentLayout_->addWidget(page);
    }

    void showHeldOrders()
    {
        contentLayout_->addWidget(UiPages::createHeldOrdersPage(heldOrders_, contentHost_,
            [this](int index) {
                if (index < 0 || index >= heldOrders_.size())
                    return;
                resumeOrder_ = heldOrders_.takeAt(index);
                hasResumeOrder_ = true;
                navigateTo(QStringLiteral("New Order"));
            },
            [this](int index) {
                if (index < 0 || index >= heldOrders_.size())
                    return;
                heldOrders_.removeAt(index);
                navigateTo(QStringLiteral("Held Orders"));
            }));
    }

    void showPlaceholder(const QString &pageName)
    {
        auto *placeholder = new QFrame(contentHost_);
        placeholder->setObjectName(QStringLiteral("placeholder"));
        placeholder->setStyleSheet(QStringLiteral(
            "QFrame#placeholder { background: white; border: 1px solid %1; border-radius: 16px; }")
            .arg(kLine));
        auto *layout = new QVBoxLayout(placeholder);
        layout->setAlignment(Qt::AlignCenter);
        auto *title = new QLabel(pageName, placeholder);
        title->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 20px; font-weight: 700; background: transparent;").arg(kInk));
        title->setAlignment(Qt::AlignCenter);
        auto *message = new QLabel(QStringLiteral("This page is a UI placeholder for now."),
            placeholder);
        message->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 12px; background: transparent;").arg(kMuted));
        message->setAlignment(Qt::AlignCenter);
        layout->addWidget(title);
        layout->addSpacing(8);
        layout->addWidget(message);
        contentLayout_->addWidget(placeholder, 1);
    }

    QComboBox *roleCombo_ = nullptr;
    QVBoxLayout *navigationLayout_ = nullptr;
    QVBoxLayout *contentLayout_ = nullptr;
    QWidget *contentHost_ = nullptr;
    QLabel *headerTitle_ = nullptr;
    QLabel *headerSubtitle_ = nullptr;
    QString currentPage_ = QStringLiteral("Dashboard");
    QVector<OrderPreview> heldOrders_;
    OrderPreview resumeOrder_;
    OrderPreview billPreview_;
    int heldOrderCounter_ = 0;
    bool hasResumeOrder_ = false;
    bool hasBillPreview_ = false;
};
}

QWidget *UiPages::createDashboardPage(QWidget *parent, const QString &initialRole)
{
    return new DashboardWindow(parent, initialRole);
}