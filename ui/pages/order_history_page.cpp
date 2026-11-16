#include "order_history_page.h"

#include <QColor>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QVBoxLayout>
#include <utility>

namespace
{
const QString kNavy = QStringLiteral("#111184");
const QString kInk = QStringLiteral("#20233B");
const QString kMuted = QStringLiteral("#777B91");
const QString kLine = QStringLiteral("#E7E9F3");

class EmptyOrdersIcon : public QWidget
{
public:
    explicit EmptyOrdersIcon(QWidget *parent = nullptr) : QWidget(parent)
    {
        setFixedSize(72, 72);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(QStringLiteral("#ECEEFF")));
        painter.drawEllipse(rect());

        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(kNavy), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawRoundedRect(QRectF(24, 17, 25, 36), 4, 4);
        painter.drawLine(QPointF(30, 27), QPointF(43, 27));
        painter.drawLine(QPointF(30, 34), QPointF(43, 34));
        painter.drawLine(QPointF(30, 41), QPointF(39, 41));
    }
};

class OrderHistoryPage : public QWidget
{
public:
    OrderHistoryPage(std::function<void()> onNewOrder, QWidget *parent)
        : QWidget(parent)
    {
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 20);
        root->setSpacing(16);

        auto *card = new QFrame(this);
        card->setObjectName(QStringLiteral("historyCard"));
        card->setStyleSheet(QStringLiteral(
            "QFrame#historyCard { background: white; border: 1px solid %1; border-radius: 16px; }")
            .arg(kLine));
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(24, 22, 24, 24);
        layout->setSpacing(0);

        auto *heading = new QHBoxLayout;
        auto *title = new QLabel(QStringLiteral("Order history"), card);
        title->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 15px; font-weight: 700; background: transparent;").arg(kInk));
        heading->addWidget(title);
        heading->addStretch(1);
        auto *count = new QLabel(QStringLiteral("0 saved orders"), card);
        count->setStyleSheet(QStringLiteral(
            "color: %1; background: #F4F5FB; border-radius: 8px;"
            " padding: 7px 10px; font-size: 10px; font-weight: 600;").arg(kMuted));
        heading->addWidget(count);
        layout->addLayout(heading);
        layout->addSpacing(1);

        auto *divider = new QFrame(card);
        divider->setFrameShape(QFrame::HLine);
        divider->setStyleSheet(QStringLiteral("color: %1;").arg(kLine));
        layout->addWidget(divider);

        auto *emptyState = new QWidget(card);
        auto *emptyLayout = new QVBoxLayout(emptyState);
        emptyLayout->setContentsMargins(12, 35, 12, 25);
        emptyLayout->setSpacing(10);
        auto *icon = new EmptyOrdersIcon(emptyState);
        emptyLayout->addWidget(icon, 0, Qt::AlignHCenter);

        auto *emptyTitle = new QLabel(QStringLiteral("No order history yet"), emptyState);
        emptyTitle->setAlignment(Qt::AlignCenter);
        emptyTitle->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 17px; font-weight: 700; background: transparent;").arg(kInk));
        emptyLayout->addWidget(emptyTitle);

        auto *description = new QLabel(QStringLiteral(
            "There are no saved orders to show. Once order storage is connected and a sale is saved,"
            " it will appear here."), emptyState);
        description->setWordWrap(true);
        description->setMaximumWidth(450);
        description->setAlignment(Qt::AlignCenter);
        description->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 11px; line-height: 1.4; background: transparent;").arg(kMuted));
        emptyLayout->addWidget(description, 0, Qt::AlignHCenter);
        emptyLayout->addSpacing(5);

        auto *newOrder = new QPushButton(QStringLiteral("Start a new order"), emptyState);
        newOrder->setCursor(Qt::PointingHandCursor);
        newOrder->setFixedHeight(40);
        newOrder->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; color: white; border: none; border-radius: 9px;"
            " padding: 0 16px; font-size: 11px; font-weight: 700; }"
            "QPushButton:hover { background: #0D0D6B; }").arg(kNavy));
        emptyLayout->addWidget(newOrder, 0, Qt::AlignHCenter);
        layout->addWidget(emptyState);
        root->addWidget(card, 1);

        auto *note = new QLabel(QStringLiteral(
            "Bill previews are not saved to Order.txt or Bill.txt yet."),
            this);
        note->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; background: transparent;").arg(kMuted));
        root->addWidget(note);

        connect(newOrder, &QPushButton::clicked, this, [onNewOrder]() {
            if (onNewOrder)
                onNewOrder();
        });
    }
};
}

QWidget *UiPages::createOrderHistoryPage(QWidget *parent,
    std::function<void()> onNewOrder)
{
    return new OrderHistoryPage(std::move(onNewOrder), parent);
}
