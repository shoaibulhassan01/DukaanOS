#include "held_orders_page.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <utility>

namespace
{
const QString kNavy = QStringLiteral("#111184");
const QString kInk = QStringLiteral("#20233B");
const QString kMuted = QStringLiteral("#777B91");
const QString kLine = QStringLiteral("#E7E9F3");

QFrame *makeCard(QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("heldCard"));
    card->setStyleSheet(QStringLiteral(
        "QFrame#heldCard { background: white; border: 1px solid %1; border-radius: 14px; }")
        .arg(kLine));
    return card;
}

QString money(double amount)
{
    return QStringLiteral("PKR %1").arg(amount, 0, 'f', 2);
}

class HeldOrdersPage : public QWidget
{
public:
    HeldOrdersPage(const QVector<OrderPreview> &orders, std::function<void(int)> onResume,
        std::function<void(int)> onDiscard, QWidget *parent)
        : QWidget(parent)
    {
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 20);
        root->setSpacing(16);

        auto *banner = new QFrame(this);
        banner->setObjectName(QStringLiteral("heldBanner"));
        banner->setMinimumHeight(116);
        banner->setStyleSheet(QStringLiteral(
            "QFrame#heldBanner { border-radius: 16px; background: qlineargradient("
            "x1:0, y1:0, x2:1, y2:1, stop:0 #111184, stop:1 #5B4BFF); }"));
        auto *bannerLayout = new QHBoxLayout(banner);
        bannerLayout->setContentsMargins(24, 20, 24, 20);
        auto *bannerText = new QVBoxLayout;
        auto *title = new QLabel(QStringLiteral("Pick up where you left off"), banner);
        title->setStyleSheet(QStringLiteral(
            "color: white; font-size: 18px; font-weight: 700; background: transparent;"));
        auto *subtitle = new QLabel(QStringLiteral(
            "Your held order previews stay here until you resume or discard them."),
            banner);
        subtitle->setWordWrap(true);
        subtitle->setStyleSheet(QStringLiteral(
            "color: rgba(255,255,255,195); font-size: 11px; background: transparent;"));
        bannerText->addWidget(title);
        bannerText->addSpacing(5);
        bannerText->addWidget(subtitle);
        bannerLayout->addLayout(bannerText, 1);
        auto *count = new QLabel(QString::number(orders.size()), banner);
        count->setAlignment(Qt::AlignCenter);
        count->setFixedSize(54, 54);
        count->setStyleSheet(QStringLiteral(
            "color: white; background: rgba(255,255,255,30); border: 1px solid"
            " rgba(255,255,255,65); border-radius: 16px; font-size: 20px; font-weight: 700;"));
        bannerLayout->addWidget(count);
        root->addWidget(banner);

        auto *card = makeCard(this);
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(18, 18, 18, 18);
        auto *heading = new QHBoxLayout;
        auto *headingTitle = new QLabel(QStringLiteral("Held orders"), card);
        headingTitle->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 14px; font-weight: 700; background: transparent;").arg(kInk));
        heading->addWidget(headingTitle);
        heading->addStretch(1);
        auto *amount = new QLabel(QStringLiteral("%1 orders").arg(orders.size()), card);
        amount->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; background: transparent;").arg(kMuted));
        heading->addWidget(amount);
        cardLayout->addLayout(heading);
        cardLayout->addSpacing(14);

        if (orders.isEmpty())
        {
            auto *empty = new QLabel(QStringLiteral(
                "<b>No held orders yet</b><br><span style='color:#777B91;'>"
                "Use Hold Order from New Order to keep a sale here.</span>"), card);
            empty->setAlignment(Qt::AlignCenter);
            empty->setMinimumHeight(170);
            empty->setStyleSheet(QStringLiteral(
                "color: %1; font-size: 12px; background: #FAFAFD;"
                " border: 1px dashed %2; border-radius: 12px;").arg(kInk, kLine));
            cardLayout->addWidget(empty);
        }
        else
        {
            auto *table = new QTableWidget(card);
            table->setColumnCount(6);
            table->setHorizontalHeaderLabels({
                QStringLiteral("ORDER"), QStringLiteral("CUSTOMER"),
                QStringLiteral("ITEMS"), QStringLiteral("HELD AT"),
                QStringLiteral("TOTAL"), QStringLiteral("ACTIONS")
            });
            table->setRowCount(orders.size());
            table->verticalHeader()->hide();
            table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
            table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
            table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
            table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
            table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
            table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
            table->horizontalHeader()->setFixedHeight(38);
            table->setShowGrid(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionMode(QAbstractItemView::NoSelection);
            table->setFocusPolicy(Qt::NoFocus);
            table->setStyleSheet(QStringLiteral(
                "QTableWidget { background: white; border: none; color: %1; }"
                "QHeaderView::section { background: #F8F8FC; color: %2; border: none;"
                " border-bottom: 1px solid %3; padding: 0 8px; font-size: 9px; font-weight: 700; }")
                .arg(kInk, kMuted, kLine));

            for (int row = 0; row < orders.size(); ++row)
            {
                const OrderPreview &order = orders.at(row);
                const QString customer = order.customerName.isEmpty()
                    ? QStringLiteral("Walk-in customer") : order.customerName;
                const QString values[] = {
                    order.id, customer, QString::number(order.itemCount),
                    order.createdAt, money(order.total)
                };
                for (int column = 0; column < 5; ++column)
                {
                    auto *cell = new QTableWidgetItem(values[column]);
                    cell->setFlags(cell->flags() & ~Qt::ItemIsEditable);
                    table->setItem(row, column, cell);
                }

                auto *actions = new QWidget(table);
                auto *actionLayout = new QHBoxLayout(actions);
                actionLayout->setContentsMargins(4, 3, 4, 3);
                actionLayout->setSpacing(6);
                auto *resume = new QPushButton(QStringLiteral("Resume"), actions);
                resume->setCursor(Qt::PointingHandCursor);
                resume->setStyleSheet(QStringLiteral(
                    "QPushButton { background: %1; color: white; border: none; border-radius: 6px;"
                    " padding: 6px 9px; font-size: 9px; font-weight: 700; }"
                    "QPushButton:hover { background: #0D0D6B; }").arg(kNavy));
                auto *discard = new QPushButton(QStringLiteral("Discard"), actions);
                discard->setCursor(Qt::PointingHandCursor);
                discard->setStyleSheet(QStringLiteral(
                    "QPushButton { background: white; color: #B42318; border: 1px solid #F2D5D2;"
                    " border-radius: 6px; padding: 5px 8px; font-size: 9px; font-weight: 600; }"
                    "QPushButton:hover { background: #FFF4F2; }"));
                actionLayout->addWidget(resume);
                actionLayout->addWidget(discard);
                table->setCellWidget(row, 5, actions);
                table->setRowHeight(row, 48);
                connect(resume, &QPushButton::clicked, this, [onResume, row]() {
                    if (onResume)
                        onResume(row);
                });
                connect(discard, &QPushButton::clicked, this, [onDiscard, row]() {
                    if (onDiscard)
                        onDiscard(row);
                });
            }
            cardLayout->addWidget(table);
        }
        root->addWidget(card);

        auto *note = new QLabel(QStringLiteral(
            "Preview mode — held orders are kept in memory only and are cleared when the app closes."),
            this);
        note->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; background: transparent;").arg(kMuted));
        root->addWidget(note);
    }
};
}

QWidget *UiPages::createHeldOrdersPage(const QVector<OrderPreview> &orders, QWidget *parent,
    std::function<void(int)> onResume, std::function<void(int)> onDiscard)
{
    return new HeldOrdersPage(orders, std::move(onResume), std::move(onDiscard), parent);
}
