#include "order_history_page.h"
#include "../theme.h"
#include "bill_page.h"
#include "saved_bill.h"

#include "../../POS/order/order_repository.h"

#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QMessageBox>
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

QString dataFilePath(const QString &fileName)
{
    const QString workingDirectoryPath = QDir::current().filePath(
        QStringLiteral("Data/%1").arg(fileName));
    if (QFileInfo::exists(workingDirectoryPath))
        return workingDirectoryPath;
    return QDir(QCoreApplication::applicationDirPath()).filePath(
        QStringLiteral("../../Data/%1").arg(fileName));
}

QString displayName(QString value)
{
    if (value == QStringLiteral("-"))
        return {};
    value.replace(QLatin1Char('_'), QLatin1Char(' '));
    return value;
}

QString displayStatus(const QString &status)
{
    if (status == QStringLiteral("paid"))
        return QStringLiteral("Paid");
    if (status == QStringLiteral("on-hold"))
        return QStringLiteral("On hold");
    if (status == QStringLiteral("in-progress"))
        return QStringLiteral("In progress");
    if (status == QStringLiteral("archived"))
        return QStringLiteral("Archived");
    return status;
}

QFrame *makeCard(QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("historyCard"));
    card->setStyleSheet(QStringLiteral(
        "QFrame#historyCard { background: white; border: 1px solid %1; border-radius: 14px; }")
        .arg(kLine));
    return card;
}

QFrame *makeStat(QWidget *parent, const QString &title, QLabel **value,
    const QString &caption, const QString &accent)
{
    auto *card = makeCard(parent);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(17, 15, 17, 15);
    layout->setSpacing(5);
    auto *heading = new QLabel(title, card);
    heading->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 10px; font-weight: 600; background: transparent;").arg(kMuted));
    *value = new QLabel(QStringLiteral("0"), card);
    (*value)->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 21px; font-weight: 700; background: transparent;").arg(kInk));
    auto *hint = new QLabel(caption, card);
    hint->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 9px; font-weight: 600; background: transparent;").arg(accent));
    layout->addWidget(heading);
    layout->addWidget(*value);
    layout->addWidget(hint);
    return card;
}

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
        painter.setBrush(UiTheme::surfaceColor(QColor(QStringLiteral("#ECEEFF"))));
        painter.drawEllipse(rect());
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(UiTheme::textColor(QColor(kNavy)), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
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
        : QWidget(parent), onNewOrder_(std::move(onNewOrder))
    {
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 20);
        root->setSpacing(16);

        auto *stats = new QGridLayout;
        stats->setHorizontalSpacing(12);
        stats->setColumnStretch(0, 1);
        stats->setColumnStretch(1, 1);
        stats->setColumnStretch(2, 1);
        stats->addWidget(makeStat(this, QStringLiteral("ORDERS TODAY"), &ordersTodayValue_,
            QStringLiteral("From saved order records"), QStringLiteral("#4F46E5")), 0, 0);
        stats->addWidget(makeStat(this, QStringLiteral("PAID ORDERS"), &paidOrdersValue_,
            QStringLiteral("Saved with paid status"), QStringLiteral("#16845B")), 0, 1);
        stats->addWidget(makeStat(this, QStringLiteral("SALES TODAY"), &salesTodayValue_,
            QStringLiteral("Paid order amounts"), QStringLiteral("#D97706")), 0, 2);
        root->addLayout(stats);

        auto *card = makeCard(this);
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(18, 18, 18, 18);

        auto *heading = new QHBoxLayout;
        auto *title = new QLabel(QStringLiteral("All orders"), card);
        title->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 14px; font-weight: 700; background: transparent;").arg(kInk));
        heading->addWidget(title);
        heading->addStretch(1);
        countLabel_ = new QLabel(card);
        countLabel_->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; background: transparent;").arg(kMuted));
        heading->addWidget(countLabel_);
        heading->addSpacing(10);
        viewBillButton_ = new QPushButton(QStringLiteral("View bill"), card);
        viewBillButton_->setCursor(Qt::PointingHandCursor);
        viewBillButton_->setFixedHeight(34);
        viewBillButton_->setEnabled(false);
        viewBillButton_->setToolTip(QStringLiteral("Show the saved bill of the selected order (or double-click it)"));
        viewBillButton_->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; color: white; border: none; border-radius: 8px;"
            " padding: 0 14px; font-size: 10px; font-weight: 700; }"
            "QPushButton:hover { background: #0D0D6B; }"
            "QPushButton:disabled { background: #E4E5EE; color: #A9ABBD; }").arg(kNavy));
        heading->addWidget(viewBillButton_);
        layout->addLayout(heading);
        layout->addSpacing(14);

        auto *filters = new QHBoxLayout;
        search_ = new QLineEdit(card);
        search_->setPlaceholderText(QStringLiteral("Search order, customer or phone"));
        search_->setFixedHeight(40);
        search_->setStyleSheet(QStringLiteral(
            "QLineEdit { background: #FAFAFD; color: %1; border: 1px solid %2;"
            " border-radius: 8px; padding: 0 12px; font-size: 11px; }"
            "QLineEdit:focus { background: white; border: 1px solid %3; }")
            .arg(kInk, kLine, kNavy));
        filters->addWidget(search_, 1);

        statusFilter_ = new QComboBox(card);
        statusFilter_->addItems({QStringLiteral("All statuses"), QStringLiteral("Paid"),
            QStringLiteral("On hold"), QStringLiteral("In progress"), QStringLiteral("Archived")});
        statusFilter_->setFixedSize(145, 40);
        statusFilter_->setStyleSheet(search_->styleSheet());
        filters->addWidget(statusFilter_);

        periodFilter_ = new QComboBox(card);
        periodFilter_->addItems({QStringLiteral("Today"), QStringLiteral("Last 7 days"),
            QStringLiteral("This month"), QStringLiteral("All time")});
        periodFilter_->setCurrentText(QStringLiteral("All time"));
        periodFilter_->setFixedSize(132, 40);
        periodFilter_->setStyleSheet(search_->styleSheet());
        filters->addWidget(periodFilter_);
        layout->addLayout(filters);
        layout->addSpacing(12);

        table_ = new QTableWidget(card);
        table_->setColumnCount(7);
        table_->setHorizontalHeaderLabels({
            QStringLiteral("ORDER ID"), QStringLiteral("CUSTOMER"),
            QStringLiteral("DATE & TIME"), QStringLiteral("ITEMS"),
            QStringLiteral("PAYMENT"), QStringLiteral("STATUS"),
            QStringLiteral("TOTAL")
        });
        table_->verticalHeader()->hide();
        table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        for (int column = 2; column < 7; ++column)
            table_->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
        table_->horizontalHeader()->setFixedHeight(38);
        table_->setShowGrid(false);
        table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table_->setSelectionBehavior(QAbstractItemView::SelectRows);
        table_->setSelectionMode(QAbstractItemView::SingleSelection);
        table_->setStyleSheet(QStringLiteral(
            "QTableWidget { background: white; border: none; color: %1; outline: 0; }"
            "QTableWidget::item { padding: 6px; border-bottom: 1px solid #F0F1F6; }"
            "QTableWidget::item:selected { background: #F0F0FF; color: %2; }"
            "QHeaderView::section { background: #F8F8FC; color: %3; border: none;"
            " border-bottom: 1px solid %4; padding: 0 7px; font-size: 9px; font-weight: 700; }")
            .arg(kInk, kNavy, kMuted, kLine));
        layout->addWidget(table_);

        emptyState_ = new QWidget(card);
        auto *emptyLayout = new QVBoxLayout(emptyState_);
        emptyLayout->setContentsMargins(12, 24, 12, 24);
        emptyLayout->setSpacing(9);
        auto *icon = new EmptyOrdersIcon(emptyState_);
        emptyLayout->addWidget(icon, 0, Qt::AlignHCenter);
        auto *emptyTitle = new QLabel(QStringLiteral("No saved orders found"), emptyState_);
        emptyTitle->setAlignment(Qt::AlignCenter);
        emptyTitle->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 16px; font-weight: 700; background: transparent;").arg(kInk));
        emptyLayout->addWidget(emptyTitle);
        auto *emptyMessage = new QLabel(QStringLiteral(
            "Orders saved to Order.txt will appear here."), emptyState_);
        emptyMessage->setAlignment(Qt::AlignCenter);
        emptyMessage->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 11px; background: transparent;").arg(kMuted));
        emptyLayout->addWidget(emptyMessage);
        auto *newOrderButton = new QPushButton(QStringLiteral("Start a new order"), emptyState_);
        newOrderButton->setCursor(Qt::PointingHandCursor);
        newOrderButton->setFixedHeight(40);
        newOrderButton->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; color: white; border: none; border-radius: 9px;"
            " padding: 0 16px; font-size: 11px; font-weight: 700; }"
            "QPushButton:hover { background: #0D0D6B; }").arg(kNavy));
        emptyLayout->addWidget(newOrderButton, 0, Qt::AlignHCenter);
        layout->addWidget(emptyState_);
        root->addWidget(card, 1);

        note_ = new QLabel(this);
        note_->setWordWrap(true);
        note_->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; background: transparent;").arg(kMuted));
        root->addWidget(note_);

        connect(newOrderButton, &QPushButton::clicked, this, [this]() {
            if (onNewOrder_)
                onNewOrder_();
        });
        connect(search_, &QLineEdit::textChanged, this, [this]() { applyFilters(); });
        connect(table_, &QTableWidget::itemSelectionChanged, this, [this]() {
            viewBillButton_->setEnabled(!table_->selectedItems().isEmpty());
        });
        connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) { showBill(row); });
        connect(viewBillButton_, &QPushButton::clicked, this, [this]() {
            const QList<QTableWidgetItem *> selected = table_->selectedItems();
            if (!selected.isEmpty())
                showBill(selected.first()->row());
        });
        connect(statusFilter_, &QComboBox::currentTextChanged,
            this, [this]() { applyFilters(); });
        connect(periodFilter_, &QComboBox::currentTextChanged,
            this, [this]() { applyFilters(); });
        loadOrders();
    }

private:
    void loadOrders()
    {
        QString error;
        loadFailed_ = !Pos::readOrders(dataFilePath(QStringLiteral("Order.txt")), orders_, error);
        if (loadFailed_)
        {
            note_->setText(error);
            note_->setStyleSheet(QStringLiteral(
                "color: #B42318; font-size: 10px; background: transparent;"));
            table_->hide();
            emptyState_->hide();
            countLabel_->setText(QStringLiteral("Could not load orders"));
            return;
        }

        rows_.clear();
        int ordersToday = 0;
        int paidOrders = 0;
        double salesToday = 0;
        const QDate today = QDate::currentDate();
        for (const Pos::Order &order : orders_)
        {
            QVector<Pos::OrderDetail> details;
            if (!Pos::readOrderDetails(dataFilePath(QStringLiteral("OrderDetail.txt")),
                    order.id, details, error))
            {
                note_->setText(error);
                note_->setStyleSheet(QStringLiteral(
                    "color: #B42318; font-size: 10px; background: transparent;"));
                table_->hide();
                emptyState_->hide();
                countLabel_->setText(QStringLiteral("Could not load order details"));
                loadFailed_ = true;
                return;
            }

            int itemCount = 0;
            for (const Pos::OrderDetail &detail : details)
                itemCount += detail.quantity;

            const QDate date = QDate::fromString(order.date, QStringLiteral("yyyy-MM-dd"));
            if (date == today)
            {
                ++ordersToday;
                if (order.status == QStringLiteral("paid"))
                    salesToday += order.paymentAmount;
            }
            if (order.status == QStringLiteral("paid"))
                ++paidOrders;

            const QDateTime dateTime(date,
                QTime::fromString(order.time, QStringLiteral("HH:mm:ss")));
            const QString dateText = dateTime.isValid()
                ? dateTime.toString(QStringLiteral("dd MMM yyyy, hh:mm AP"))
                : QStringLiteral("%1 %2").arg(order.date, order.time);
            const QString payment = displayName(order.paymentMode);
            rows_.append({
                order.id,
                displayName(order.customerName).isEmpty()
                    ? QStringLiteral("Walk-in customer") : displayName(order.customerName),
                dateText,
                QString::number(itemCount),
                payment.isEmpty() ? QStringLiteral("-") : payment,
                displayStatus(order.status),
                QStringLiteral("PKR %1").arg(order.paymentAmount, 0, 'f', 2)
            });
            rowDates_.append(date);
            rowSearchText_.append(QStringLiteral("%1 %2 %3")
                .arg(order.id, displayName(order.customerName), displayName(order.customerContact))
                .toLower());
        }

        ordersTodayValue_->setText(QString::number(ordersToday));
        paidOrdersValue_->setText(QString::number(paidOrders));
        salesTodayValue_->setText(QStringLiteral("PKR %1").arg(salesToday, 0, 'f', 2));
        note_->setText(QStringLiteral(
            "Showing saved orders from Order.txt; item quantities are read from OrderDetail.txt."));
        applyFilters();
    }

    void applyFilters()
    {
        if (loadFailed_)
            return;

        table_->setRowCount(rows_.size());
        const QString query = search_->text().trimmed().toLower();
        const QString selectedStatus = statusFilter_->currentText();
        const QString selectedPeriod = periodFilter_->currentText();
        const QDate today = QDate::currentDate();
        int visibleRows = 0;

        for (int row = 0; row < rows_.size(); ++row)
        {
            for (int column = 0; column < rows_.at(row).size(); ++column)
            {
                auto *cell = new QTableWidgetItem(rows_.at(row).at(column));
                cell->setFlags(cell->flags() & ~Qt::ItemIsEditable);
                if (column == 5)
                {
                    const QString status = rows_.at(row).at(column);
                    cell->setForeground(UiTheme::textColor(QColor(status == QStringLiteral("Paid")
                        ? QStringLiteral("#18764A")
                        : status == QStringLiteral("On hold")
                            ? QStringLiteral("#B7791F") : QStringLiteral("#B42318"))));
                }
                table_->setItem(row, column, cell);
            }
            table_->setRowHeight(row, 43);

            const QString rowStatus = rows_.at(row).at(5);
            const bool matchesSearch = query.isEmpty() || rowSearchText_.at(row).contains(query);
            const bool matchesStatus = selectedStatus == QStringLiteral("All statuses") ||
                rowStatus.compare(selectedStatus, Qt::CaseInsensitive) == 0;
            const int daysAgo = rowDates_.at(row).daysTo(today);
            const bool matchesPeriod =
                selectedPeriod == QStringLiteral("All time") ||
                (selectedPeriod == QStringLiteral("Today") && rowDates_.at(row) == today) ||
                (selectedPeriod == QStringLiteral("Last 7 days") && daysAgo >= 0 && daysAgo < 7) ||
                (selectedPeriod == QStringLiteral("This month") &&
                    rowDates_.at(row).year() == today.year() &&
                    rowDates_.at(row).month() == today.month());
            const bool visible = matchesSearch && matchesStatus && matchesPeriod;
            table_->setRowHidden(row, !visible);
            if (visible)
                ++visibleRows;
        }

        const bool hasVisibleRows = visibleRows > 0;
        table_->setVisible(hasVisibleRows);
        emptyState_->setVisible(!hasVisibleRows);
        countLabel_->setText(QStringLiteral("%1 of %2 orders")
            .arg(visibleRows).arg(orders_.size()));
        if (orders_.isEmpty())
            note_->setText(QStringLiteral("Order.txt currently contains no saved orders."));
        else
            note_->setText(QStringLiteral(
                "History is loaded from Order.txt and OrderDetail.txt. Bill totals come from saved order payments."));
    }

    // Table rows are built in the same order as orders_.
    void showBill(int row)
    {
        if (row < 0 || row >= orders_.size())
            return;
        OrderPreview bill;
        QString error;
        if (!UiPages::loadSavedBill(orders_.at(row), bill, error))
        {
            QMessageBox::information(this, QStringLiteral("No bill"), error);
            return;
        }
        UiPages::showBillDialog(bill, this);
    }

    std::function<void()> onNewOrder_;
    bool loadFailed_ = false;
    QPushButton *viewBillButton_ = nullptr;
    QVector<Pos::Order> orders_;
    QVector<QStringList> rows_;
    QVector<QDate> rowDates_;
    QVector<QString> rowSearchText_;
    QLabel *ordersTodayValue_ = nullptr;
    QLabel *paidOrdersValue_ = nullptr;
    QLabel *salesTodayValue_ = nullptr;
    QLabel *countLabel_ = nullptr;
    QLabel *note_ = nullptr;
    QLineEdit *search_ = nullptr;
    QComboBox *statusFilter_ = nullptr;
    QComboBox *periodFilter_ = nullptr;
    QTableWidget *table_ = nullptr;
    QWidget *emptyState_ = nullptr;
};
}

QWidget *UiPages::createOrderHistoryPage(QWidget *parent,
    std::function<void()> onNewOrder)
{
    return new OrderHistoryPage(std::move(onNewOrder), parent);
}
