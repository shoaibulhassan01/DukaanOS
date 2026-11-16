#include "bill_page.h"

#include <QDateTime>
#include <QColor>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
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

QString money(double amount)
{
    return QStringLiteral("PKR %1").arg(amount, 0, 'f', 2);
}

QLabel *makeLabel(const QString &text, const QString &style, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setStyleSheet(style);
    return label;
}

QWidget *createBillContents(const OrderPreview &order, QWidget *parent)
{
    auto *receipt = new QFrame(parent);
    receipt->setObjectName(QStringLiteral("billReceipt"));
    receipt->setStyleSheet(QStringLiteral(
        "QFrame#billReceipt { background: white; border: 1px solid %1; border-radius: 16px; }")
        .arg(kLine));
    auto *layout = new QVBoxLayout(receipt);
    layout->setContentsMargins(28, 26, 28, 24);
    layout->setSpacing(0);

    auto *brandRow = new QHBoxLayout;
    brandRow->addWidget(makeLabel(QStringLiteral("DukaanOS"),
        QStringLiteral("color: %1; font-size: 19px; font-weight: 800; background: transparent;")
            .arg(kNavy), receipt));
    brandRow->addStretch(1);
    brandRow->addWidget(makeLabel(QStringLiteral("PAYMENT PREVIEW"),
        QStringLiteral("color: #18764A; background: #E8F8F0; border-radius: 8px;"
                       " padding: 7px 10px; font-size: 9px; font-weight: 700;"), receipt));
    layout->addLayout(brandRow);
    layout->addSpacing(7);
    layout->addWidget(makeLabel(QStringLiteral("Sales receipt"),
        QStringLiteral("color: %1; font-size: 12px; background: transparent;").arg(kMuted),
        receipt));
    layout->addSpacing(20);

    auto *details = new QGridLayout;
    details->setHorizontalSpacing(32);
    details->setVerticalSpacing(12);
    const QString keyStyle = QStringLiteral(
        "color: %1; font-size: 9px; font-weight: 700; background: transparent;").arg(kMuted);
    const QString valueStyle = QStringLiteral(
        "color: %1; font-size: 11px; font-weight: 600; background: transparent;").arg(kInk);
    details->addWidget(makeLabel(QStringLiteral("BILL NUMBER"), keyStyle, receipt), 0, 0);
    details->addWidget(makeLabel(QStringLiteral("DATE & TIME"), keyStyle, receipt), 0, 1);
    details->addWidget(makeLabel(order.id.isEmpty() ? QStringLiteral("PREVIEW") : order.id,
        valueStyle, receipt), 1, 0);
    details->addWidget(makeLabel(order.createdAt.isEmpty()
            ? QDateTime::currentDateTime().toString(QStringLiteral("dd MMM yyyy, hh:mm AP"))
            : order.createdAt, valueStyle, receipt), 1, 1);
    details->addWidget(makeLabel(QStringLiteral("CUSTOMER"), keyStyle, receipt), 2, 0);
    details->addWidget(makeLabel(QStringLiteral("PAYMENT METHOD"), keyStyle, receipt), 2, 1);
    details->addWidget(makeLabel(order.customerName.isEmpty()
            ? QStringLiteral("Walk-in customer") : order.customerName.toHtmlEscaped(),
        valueStyle, receipt), 3, 0);
    details->addWidget(makeLabel(order.paymentMethod.isEmpty()
            ? QStringLiteral("Cash") : order.paymentMethod, valueStyle, receipt), 3, 1);
    if (!order.phone.isEmpty())
    {
        details->addWidget(makeLabel(QStringLiteral("PHONE"), keyStyle, receipt), 4, 0);
        details->addWidget(makeLabel(order.phone.toHtmlEscaped(), valueStyle, receipt), 5, 0);
    }
    layout->addLayout(details);
    layout->addSpacing(22);

    auto *table = new QTableWidget(receipt);
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels({
        QStringLiteral("ITEM"), QStringLiteral("PRICE"), QStringLiteral("QTY"),
        QStringLiteral("AMOUNT")
    });
    table->setRowCount(order.lines.size());
    table->verticalHeader()->hide();
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column = 1; column < 4; ++column)
        table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setFixedHeight(36);
    table->setShowGrid(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setFocusPolicy(Qt::NoFocus);
    table->setMinimumHeight(qMax(80, order.lines.size() * 38 + 36));
    table->setStyleSheet(QStringLiteral(
        "QTableWidget { background: white; border: none; color: %1; }"
        "QHeaderView::section { background: #F8F8FC; color: %2; border: none;"
        " border-bottom: 1px solid %3; padding: 0 8px; font-size: 9px; font-weight: 700; }")
        .arg(kInk, kMuted, kLine));
    for (int row = 0; row < order.lines.size(); ++row)
    {
        const OrderPreviewLine &line = order.lines.at(row);
        QString name = line.product.name;
        name.replace(QLatin1Char('_'), QLatin1Char(' '));
        const double amount = line.product.unitPrice * line.quantity;
        const QString values[] = {
            name, money(line.product.unitPrice), QString::number(line.quantity), money(amount)
        };
        for (int column = 0; column < 4; ++column)
        {
            auto *cell = new QTableWidgetItem(values[column]);
            cell->setFlags(cell->flags() & ~Qt::ItemIsEditable);
            table->setItem(row, column, cell);
        }
        table->setRowHeight(row, 38);
    }
    layout->addWidget(table);
    layout->addSpacing(18);

    auto *totals = new QGridLayout;
    totals->setHorizontalSpacing(20);
    totals->setVerticalSpacing(9);
    auto addTotal = [receipt, totals, &keyStyle, &valueStyle](int row,
                         const QString &label, const QString &value) {
        totals->addWidget(makeLabel(label, keyStyle, receipt), row, 0);
        auto *amount = makeLabel(value, valueStyle, receipt);
        amount->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        totals->addWidget(amount, row, 1);
    };
    addTotal(0, QStringLiteral("Subtotal"), money(order.subtotal));
    addTotal(1, QStringLiteral("Tax"), money(order.tax));
    addTotal(2, QStringLiteral("Discount"), QStringLiteral("- %1").arg(money(order.discount)));
    auto *divider = new QFrame(receipt);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet(QStringLiteral("color: %1;").arg(kLine));
    totals->addWidget(divider, 3, 0, 1, 2);
    auto *totalLabel = makeLabel(QStringLiteral("TOTAL PAID"),
        QStringLiteral("color: %1; font-size: 11px; font-weight: 800; background: transparent;")
            .arg(kInk), receipt);
    auto *totalValue = makeLabel(money(order.total),
        QStringLiteral("color: %1; font-size: 17px; font-weight: 800; background: transparent;")
            .arg(kNavy), receipt);
    totalValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    totals->addWidget(totalLabel, 4, 0);
    totals->addWidget(totalValue, 4, 1);
    layout->addLayout(totals);
    layout->addSpacing(20);
    auto *footer = makeLabel(QStringLiteral("Thank you for shopping with us!"),
        QStringLiteral("color: %1; font-size: 11px; font-weight: 600;"
                       " background: transparent; padding-top: 12px;"
                       " border-top: 1px dashed %2;").arg(kMuted, kLine), receipt);
    footer->setAlignment(Qt::AlignCenter);
    layout->addWidget(footer);
    return receipt;
}

class BillPage : public QWidget
{
public:
    BillPage(const OrderPreview &order, std::function<void()> onNewOrder, QWidget *parent)
        : QWidget(parent)
    {
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 20);
        root->setSpacing(14);

        auto *actions = new QHBoxLayout;
        auto *caption = makeLabel(QStringLiteral("Your bill is ready to review"),
            QStringLiteral("color: %1; font-size: 12px; background: transparent;").arg(kMuted),
            this);
        actions->addWidget(caption);
        actions->addStretch(1);
        auto *newOrder = new QPushButton(QStringLiteral("New order"), this);
        newOrder->setCursor(Qt::PointingHandCursor);
        newOrder->setFixedHeight(38);
        newOrder->setStyleSheet(QStringLiteral(
            "QPushButton { background: white; color: %1; border: 1px solid %2;"
            " border-radius: 9px; padding: 0 15px; font-size: 11px; font-weight: 600; }"
            "QPushButton:hover { background: #F8F8FC; }").arg(kNavy, kLine));
        actions->addWidget(newOrder);
        auto *print = new QPushButton(QStringLiteral("Print bill"), this);
        print->setCursor(Qt::PointingHandCursor);
        print->setFixedHeight(38);
        print->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; color: white; border: none; border-radius: 9px;"
            " padding: 0 16px; font-size: 11px; font-weight: 700; }"
            "QPushButton:hover { background: #0D0D6B; }").arg(kNavy));
        actions->addWidget(print);
        root->addLayout(actions);

        auto *receiptWrap = new QWidget(this);
        auto *receiptLayout = new QHBoxLayout(receiptWrap);
        receiptLayout->setContentsMargins(0, 0, 0, 0);
        receiptLayout->addStretch(1);
        receiptLayout->addWidget(createBillContents(order, receiptWrap), 1);
        receiptLayout->addStretch(1);
        root->addWidget(receiptWrap, 1);

        auto *note = makeLabel(QStringLiteral(
            "Preview only — this bill has not been saved or sent to a printer."),
            QStringLiteral("color: %1; font-size: 10px; background: transparent;").arg(kMuted),
            this);
        note->setAlignment(Qt::AlignCenter);
        root->addWidget(note);

        connect(newOrder, &QPushButton::clicked, this, [onNewOrder]() {
            if (onNewOrder)
                onNewOrder();
        });
        connect(print, &QPushButton::clicked, this, [this]() {
            QMessageBox::information(this, QStringLiteral("Print bill"),
                QStringLiteral("Printing is not connected in preview mode."));
        });
    }
};
}

QWidget *UiPages::createBillPage(const OrderPreview &order, QWidget *parent,
    std::function<void()> onNewOrder)
{
    return new BillPage(order, std::move(onNewOrder), parent);
}
