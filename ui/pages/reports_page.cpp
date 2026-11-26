#include "reports_page.h"

#include "../../POS/category/category_repository.h"
#include "../../POS/common/data_paths.h"
#include "../../POS/order/order_service.h"
#include "../../POS/product/product_service.h"
#include "../../POS/report/report_service.h"

#include <QComboBox>
#include <QDateEdit>
#include <QFrame>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <functional>

namespace
{
const QString kNavy = QStringLiteral("#111184");
const QString kInk = QStringLiteral("#20233B");
const QString kMuted = QStringLiteral("#777B91");
const QString kLine = QStringLiteral("#E7E9F3");

// One report as a table.
struct ReportTable
{
    QStringList columns;
    QVector<QStringList> rows;
    QList<int> rightAligned;   // columns with numbers
    QString footer;            // e.g. totals; may be empty
    QString emptyText;         // shown when there are no rows
};

// What the manager chose above the table.
struct ReportOptions
{
    QString unit;      // "" for all items, "count" or "weight" (unit filter)
    QDate from;        // sales period (both days included)
    QDate to;
};

struct ReportDefinition
{
    QString title;
    QString description;
    bool usesUnitFilter = false;
    std::function<bool(ReportTable &table, QString &error, const ReportOptions &options)> build;
    bool usesPeriod = false;   // shows the Daily / Weekly / ... period row
};

QString display(QString field)
{
    return field.replace(QLatin1Char('_'), QLatin1Char(' '));
}

QHash<QString, QString> categoryNames()
{
    QHash<QString, QString> names;
    QVector<Pos::Category> categories;
    QString error;
    if (Pos::readCategories(Pos::DataPaths::categories(), categories, error))
        for (const Pos::Category &category : categories)
            names.insert(category.id, display(category.name));
    return names;
}

QString categoryText(const QHash<QString, QString> &names, const QString &id)
{
    return names.value(id, QStringLiteral("Not found"));
}

QString amountText(double amount)
{
    return QStringLiteral("PKR %1").arg(amount, 0, 'f', 2);
}

QString dayText(const QDate &date)
{
    return date.toString(QStringLiteral("dd MMM yyyy"));
}

QString paymentText(QString mode)
{
    if (!mode.isEmpty())
        mode[0] = mode.at(0).toUpper();
    return mode;
}

void addOrderReportRows(ReportTable &table, const Pos::OrderReport &report)
{
    table.columns = {QStringLiteral("ORDER ID"), QStringLiteral("CUSTOMER"),
                     QStringLiteral("DATE & TIME"), QStringLiteral("STATUS"),
                     QStringLiteral("PAYMENT"), QStringLiteral("TOTAL")};
    table.rightAligned = {5};
    for (const Pos::Order &order : report.orders)
    {
        table.rows.append({
            order.id,
            display(order.customerName),
            QStringLiteral("%1  %2").arg(order.date, order.time),
            display(order.status),
            display(order.paymentMode),
            amountText(order.paymentAmount)});
    }
    table.footer = QStringLiteral("%1 order(s)  \u00B7  Total: %2")
        .arg(report.orders.size()).arg(amountText(report.totalAmount));
    table.emptyText = QStringLiteral("There are no orders in this report.");
}

// ---------------------------------------------------------------------------
// The reports (req 11). To add one, put its calculation in POS/report/ and
// add one entry to this list; the page does the rest.
// ---------------------------------------------------------------------------
QVector<ReportDefinition> reports()
{
    return {
        {QStringLiteral("Top 10 most sold items"),
         QStringLiteral("Items sold in paid orders, most sold first, with the stock that is left. "
                        "Held and archived orders are not counted."),
         true,
         [](ReportTable &table, QString &error, const ReportOptions &options) {
             // Owner: Umar Khan
             const QString &unit = options.unit;
             QVector<Pos::SoldItem> items;
             if (!Pos::topSoldProducts(Pos::DataPaths::products(), Pos::DataPaths::orders(),
                     Pos::DataPaths::orderDetail(), 10, items, error, unit))
                 return false;
             const QHash<QString, QString> names = categoryNames();
             table.columns = {QStringLiteral("#"), QStringLiteral("PRODUCT"), QStringLiteral("CATEGORY"),
                              QStringLiteral("QUANTITY SOLD"), QStringLiteral("REMAINING STOCK")};
             table.rightAligned = {0, 3, 4};
             int total = 0;
             for (int i = 0; i < items.size(); ++i)
             {
                 const Pos::SoldItem &item = items.at(i);
                 total += item.quantitySold;
                 table.rows.append({
                     QString::number(i + 1),
                     QStringLiteral("%1  (%2)").arg(display(item.product.name), item.product.id),
                     item.inProductFile ? categoryText(names, item.product.categoryId) : QStringLiteral("-"),
                     Pos::quantityText(item.quantitySold, item.product.unit),
                     item.inProductFile
                         ? Pos::quantityText(item.product.quantity, item.product.unit)
                         : QStringLiteral("no longer in Product.txt")});
             }
             // A total only makes sense when every row has the same unit.
             table.footer = items.isEmpty() ? QString()
                 : unit.isEmpty() ? QStringLiteral("%1 item(s) shown").arg(items.size())
                 : QStringLiteral("%1 item(s) shown \u00B7 %2 sold in total").arg(items.size())
                       .arg(Pos::quantityText(total, unit));
             table.emptyText = QStringLiteral("Nothing has been sold yet: there are no paid orders.");
             return true;
         }},
        {QStringLiteral("Top 10 items by remaining stock"),
         QStringLiteral("Items with the most stock left, highest first, so that purchase of these is not initiated."),
         true,
         [](ReportTable &table, QString &error, const ReportOptions &options) {
             // Owner: Umar Khan
             const QString &unit = options.unit;
             QVector<Pos::Product> products;
             if (!Pos::topStockProducts(Pos::DataPaths::products(), 10, products, error, unit))
                 return false;
             const QHash<QString, QString> names = categoryNames();
             table.columns = {QStringLiteral("#"), QStringLiteral("PRODUCT"), QStringLiteral("CATEGORY"),
                              QStringLiteral("REMAINING STOCK")};
             table.rightAligned = {0, 3};
             for (int i = 0; i < products.size(); ++i)
             {
                 const Pos::Product &product = products.at(i);
                 table.rows.append({
                     QString::number(i + 1),
                     QStringLiteral("%1  (%2)").arg(display(product.name), product.id),
                     categoryText(names, product.categoryId),
                     Pos::quantityText(product.quantity, product.unit)});
             }
             table.footer = products.isEmpty() ? QString()
                 : QStringLiteral("%1 item(s) shown").arg(products.size());
             table.emptyText = QStringLiteral("There are no products in Product.txt.");
             return true;
         }},
        {QStringLiteral("Incomplete orders"),
         QStringLiteral("Orders that are not paid or archived (on-hold orders). Each is shown at what it would cost if paid today (current prices and seasonal discount)."),
         false,
         [](ReportTable &table, QString &error, const ReportOptions &) {
             Pos::OrderReport report;
             if (!Pos::readIncompleteOrdersReport(report, error))
                 return false;
             addOrderReportRows(table, report);
             table.emptyText = QStringLiteral("There are no incomplete orders.");
             return true;
         }},
        {QStringLiteral("Completed orders"),
         QStringLiteral("Paid and archived orders, with their total amount."),
         false,
         [](ReportTable &table, QString &error, const ReportOptions &) {
             Pos::OrderReport report;
             if (!Pos::readCompletedOrdersReport(report, error))
                 return false;
             addOrderReportRows(table, report);
             table.emptyText = QStringLiteral("There are no completed orders.");
             return true;
         }},
        {QStringLiteral("Cash sales"),
         QStringLiteral("Paid orders paid in cash, by bill date, for the chosen day, week (Monday to Sunday), "
                        "month, year or period, with their total."),
         false,
         [](ReportTable &table, QString &error, const ReportOptions &options) {
             // Owner: Umar Khan (req 11: daily, weekly, monthly, yearly, specified period)
             Pos::SalesReport report;
             if (!Pos::cashSalesReport(Pos::DataPaths::orders(), Pos::DataPaths::bills(),
                     options.from, options.to, report, error))
                 return false;
             table.columns = {QStringLiteral("BILL ID"), QStringLiteral("CUSTOMER"),
                              QStringLiteral("ORDER ID"), QStringLiteral("DATE & TIME"),
                              QStringLiteral("AMOUNT")};
             table.rightAligned = {4};
             for (const Pos::Sale &sale : report.sales)
                 table.rows.append({
                     sale.bill.id,
                     display(sale.bill.customerName),
                     sale.bill.orderId,
                     QStringLiteral("%1  %2").arg(sale.bill.date, sale.bill.time),
                     amountText(sale.bill.totalPayable)});
             table.footer = QStringLiteral("%1 cash sale(s)  \u00B7  Total: %2")
                 .arg(report.sales.size()).arg(amountText(report.totalAmount));
             table.emptyText = QStringLiteral("There are no cash sales in this period.");
             return true;
         },
         true},
        {QStringLiteral("All sales"),
         QStringLiteral("Every paid order (cash, card and wallet) with its amounts and discounts, "
                        "and the total amount and discounts of all sales."),
         false,
         [](ReportTable &table, QString &error, const ReportOptions &) {
             // Owner: Umar Khan (req 11: all sales with total amount and discounts)
             Pos::SalesReport report;
             if (!Pos::allSalesReport(Pos::DataPaths::orders(), Pos::DataPaths::bills(), report, error))
                 return false;
             table.columns = {QStringLiteral("BILL ID"), QStringLiteral("CUSTOMER"),
                              QStringLiteral("DATE & TIME"), QStringLiteral("PAYMENT"),
                              QStringLiteral("BEFORE TAX"), QStringLiteral("TAX"),
                              QStringLiteral("PRODUCT DISCOUNT"), QStringLiteral("SEASONAL DISCOUNT"),
                              QStringLiteral("SPECIAL DISCOUNT"), QStringLiteral("TOTAL")};
             table.rightAligned = {4, 5, 6, 7, 8, 9};
             for (const Pos::Sale &sale : report.sales)
                 table.rows.append({
                     sale.bill.id,
                     display(sale.bill.customerName),
                     QStringLiteral("%1  %2").arg(sale.bill.date, sale.bill.time),
                     paymentText(sale.paymentMode),
                     amountText(sale.bill.totalWithoutTax),
                     amountText(sale.bill.totalTax),
                     amountText(sale.bill.totalDiscount),
                     amountText(sale.bill.seasonalDiscount),
                     amountText(sale.bill.specialDiscount),
                     amountText(sale.bill.totalPayable)});
             table.footer = QStringLiteral(
                 "%1 sale(s)  \u00B7  Total amount: %2  \u00B7  Discounts: %3 "
                 "(product %4, seasonal %5, special %6)")
                 .arg(report.sales.size())
                 .arg(amountText(report.totalAmount), amountText(report.totalDiscounts),
                     amountText(report.productDiscounts), amountText(report.seasonalDiscounts),
                     amountText(report.specialDiscounts));
             table.emptyText = QStringLiteral("There are no sales yet: there are no paid orders.");
             return true;
         }},
    };
}

class ReportsPage : public QWidget
{
public:
    explicit ReportsPage(QWidget *parent) : QWidget(parent), reports_(reports())
    {
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 18);
        root->setSpacing(12);

        auto *card = new QFrame(this);
        card->setObjectName(QStringLiteral("reportsCard"));
        card->setStyleSheet(QStringLiteral(
            "QFrame#reportsCard { background: white; border: 1px solid %1; border-radius: 14px; }")
            .arg(kLine));
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(18, 18, 18, 16);
        layout->setSpacing(10);

        auto *heading = new QHBoxLayout;
        auto *title = new QLabel(QStringLiteral("Manager reports"), card);
        title->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 15px; font-weight: 700; background: transparent;").arg(kInk));
        heading->addWidget(title);
        heading->addStretch(1);

        selector_ = new QComboBox(card);
        selector_->setObjectName(QStringLiteral("reportSelector"));
        // Same look as the filters on Order History: the standard drop-down.
        selector_->setFixedSize(260, 40);
        selector_->setCursor(Qt::PointingHandCursor);
        for (const ReportDefinition &report : reports_)
            selector_->addItem(report.title);
        heading->addWidget(selector_);

        // Counts and kilograms cannot be compared, so items can be ranked
        // per unit. "All items" is the list the requirement asks for.
        unitFilter_ = new QComboBox(card);
        unitFilter_->setObjectName(QStringLiteral("reportUnitFilter"));
        unitFilter_->setFixedSize(170, 40);
        unitFilter_->setCursor(Qt::PointingHandCursor);
        unitFilter_->addItem(QStringLiteral("All items"), QString());
        unitFilter_->addItem(QStringLiteral("Counted items"), QStringLiteral("count"));
        unitFilter_->addItem(QStringLiteral("Weighed items (kg)"), QStringLiteral("weight"));
        heading->addWidget(unitFilter_);
        layout->addLayout(heading);

        // Sales period (cash sales): a calendar day, week (Monday to
        // Sunday), month or year around the chosen date, or any from-to range.
        periodRow_ = new QWidget(card);
        periodRow_->setObjectName(QStringLiteral("salesPeriodRow"));
        periodRow_->setStyleSheet(QStringLiteral("QWidget#salesPeriodRow { background: transparent; }"));
        auto *period = new QHBoxLayout(periodRow_);
        period->setContentsMargins(0, 0, 0, 0);
        period->setSpacing(8);
        auto caption = [this](const QString &text) {
            auto *label = new QLabel(text, periodRow_);
            label->setStyleSheet(QStringLiteral(
                "color: %1; font-size: 11px; font-weight: 600; background: transparent;").arg(kInk));
            return label;
        };
        auto dateBox = [this](const QString &name, const QDate &date) {
            auto *box = new QDateEdit(date, periodRow_);
            box->setObjectName(name);
            box->setCalendarPopup(true);
            box->setDisplayFormat(QStringLiteral("dd MMM yyyy"));
            box->setFixedSize(140, 40);
            return box;
        };
        periodKind_ = new QComboBox(periodRow_);
        periodKind_->setObjectName(QStringLiteral("salesPeriod"));
        periodKind_->setFixedSize(170, 40);
        periodKind_->setCursor(Qt::PointingHandCursor);
        periodKind_->addItem(QStringLiteral("Daily"), static_cast<int>(Pos::SalesPeriod::Daily));
        periodKind_->addItem(QStringLiteral("Weekly"), static_cast<int>(Pos::SalesPeriod::Weekly));
        periodKind_->addItem(QStringLiteral("Monthly"), static_cast<int>(Pos::SalesPeriod::Monthly));
        periodKind_->addItem(QStringLiteral("Yearly"), static_cast<int>(Pos::SalesPeriod::Yearly));
        periodKind_->addItem(QStringLiteral("Specified period"), -1);
        const QDate today = QDate::currentDate();
        dateLabel_ = caption(QStringLiteral("Date"));
        periodDate_ = dateBox(QStringLiteral("salesPeriodDate"), today);
        fromLabel_ = caption(QStringLiteral("From"));
        periodFrom_ = dateBox(QStringLiteral("salesPeriodFrom"), QDate(today.year(), today.month(), 1));
        toLabel_ = caption(QStringLiteral("To"));
        periodTo_ = dateBox(QStringLiteral("salesPeriodTo"), today);
        period->addWidget(caption(QStringLiteral("Period")));
        period->addWidget(periodKind_);
        period->addSpacing(8);
        period->addWidget(dateLabel_);
        period->addWidget(periodDate_);
        period->addWidget(fromLabel_);
        period->addWidget(periodFrom_);
        period->addWidget(toLabel_);
        period->addWidget(periodTo_);
        period->addStretch(1);
        layout->addWidget(periodRow_);

        description_ = new QLabel(card);
        description_->setWordWrap(true);
        description_->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; background: transparent;").arg(kMuted));
        layout->addWidget(description_);

        table_ = new QTableWidget(card);
        table_->verticalHeader()->hide();
        table_->horizontalHeader()->setFixedHeight(40);
        table_->setShowGrid(false);
        table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table_->setSelectionBehavior(QAbstractItemView::SelectRows);
        table_->setSelectionMode(QAbstractItemView::SingleSelection);
        table_->setStyleSheet(QStringLiteral(
            "QTableWidget { background: white; border: none; color: %1; outline: 0; }"
            "QTableWidget::item { padding: 5px 8px; border-bottom: 1px solid #F0F1F6; }"
            "QTableWidget::item:selected { background: #F0F0FF; color: %2; }"
            "QHeaderView::section { background: #F8F8FC; color: %3; border: none;"
            " border-bottom: 1px solid %4; padding: 0 8px; font-size: 9px; font-weight: 700; }")
            .arg(kInk, kNavy, kMuted, kLine));
        layout->addWidget(table_, 1);

        emptyLabel_ = new QLabel(card);
        emptyLabel_->setAlignment(Qt::AlignCenter);
        emptyLabel_->setWordWrap(true);
        emptyLabel_->setMinimumHeight(160);
        emptyLabel_->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 12px; background: #FAFAFD;"
            " border: 1px dashed %2; border-radius: 12px; padding: 18px;").arg(kMuted, kLine));
        // Takes the table's space when there are no rows, so the title and
        // description stay at the top instead of spreading out.
        layout->addWidget(emptyLabel_, 1);

        footer_ = new QLabel(card);
        footer_->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; font-weight: 600; background: transparent;").arg(kInk));
        layout->addWidget(footer_);
        root->addWidget(card, 1);

        connect(selector_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { refreshReport(); });
        connect(unitFilter_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { refreshReport(); });
        connect(periodKind_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { refreshReport(); });
        for (QDateEdit *box : {periodDate_, periodFrom_, periodTo_})
            connect(box, &QDateEdit::dateChanged, this, [this]() { refreshReport(); });
        refreshReport();
    }

private:
    // The empty / error box: muted text, or red for an error. Built from the
    // light colours each time (the theme turns them dark in dark mode).
    void setEmptyStyle(bool isError)
    {
        emptyLabel_->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 12px; background: #FAFAFD;"
            " border: 1px dashed %2; border-radius: 12px; padding: 18px;")
            .arg(isError ? QStringLiteral("#B42318") : kMuted, kLine));
    }

    void refreshReport()
    {
        const int index = selector_->currentIndex();
        if (index < 0 || index >= reports_.size())
            return;
        const ReportDefinition &report = reports_.at(index);
        unitFilter_->setVisible(report.usesUnitFilter);

        ReportOptions options;
        options.unit = unitFilter_->currentData().toString();
        const int kind = periodKind_->currentData().toInt();
        const bool specified = kind < 0;
        if (specified)
        {
            options.from = periodFrom_->date();
            options.to = periodTo_->date();
        }
        else
        {
            Pos::salesPeriodRange(static_cast<Pos::SalesPeriod>(kind), periodDate_->date(),
                options.from, options.to);
        }
        periodRow_->setVisible(report.usesPeriod);
        dateLabel_->setVisible(!specified);
        periodDate_->setVisible(!specified);
        fromLabel_->setVisible(specified);
        periodFrom_->setVisible(specified);
        toLabel_->setVisible(specified);
        periodTo_->setVisible(specified);
        description_->setText(!report.usesPeriod ? report.description
            : QStringLiteral("%1 Showing %2.").arg(report.description,
                  options.from == options.to ? dayText(options.from)
                      : QStringLiteral("%1 \u2013 %2").arg(dayText(options.from), dayText(options.to))));

        ReportTable result;
        QString error;
        const bool ok = report.build(result, error, options);
        table_->clear();
        table_->setRowCount(0);
        if (!ok)
        {
            table_->hide();
            emptyLabel_->setText(QStringLiteral("The report could not be made: %1").arg(error));
            setEmptyStyle(true);
            emptyLabel_->show();
            footer_->clear();
            return;
        }
        setEmptyStyle(false);

        table_->setColumnCount(result.columns.size());
        table_->setHorizontalHeaderLabels(result.columns);
        for (int column = 0; column < result.columns.size(); ++column)
            table_->horizontalHeader()->setSectionResizeMode(column,
                column == 1 ? QHeaderView::Stretch : QHeaderView::ResizeToContents);
        table_->setRowCount(result.rows.size());
        for (int row = 0; row < result.rows.size(); ++row)
        {
            for (int column = 0; column < result.columns.size(); ++column)
            {
                auto *cell = new QTableWidgetItem(result.rows.at(row).value(column));
                cell->setFlags(cell->flags() & ~Qt::ItemIsEditable);
                cell->setTextAlignment(Qt::AlignVCenter |
                    (result.rightAligned.contains(column) ? Qt::AlignRight : Qt::AlignLeft));
                table_->setItem(row, column, cell);
            }
            table_->setRowHeight(row, 40);
        }
        table_->setVisible(!result.rows.isEmpty());
        emptyLabel_->setVisible(result.rows.isEmpty());
        emptyLabel_->setText(result.emptyText);
        footer_->setText(result.footer);
        footer_->setVisible(!result.footer.isEmpty());
    }

    QVector<ReportDefinition> reports_;
    QComboBox *selector_ = nullptr;
    QComboBox *unitFilter_ = nullptr;
    QWidget *periodRow_ = nullptr;
    QComboBox *periodKind_ = nullptr;
    QLabel *dateLabel_ = nullptr;
    QDateEdit *periodDate_ = nullptr;
    QLabel *fromLabel_ = nullptr;
    QDateEdit *periodFrom_ = nullptr;
    QLabel *toLabel_ = nullptr;
    QDateEdit *periodTo_ = nullptr;
    QLabel *description_ = nullptr;
    QTableWidget *table_ = nullptr;
    QLabel *emptyLabel_ = nullptr;
    QLabel *footer_ = nullptr;
};
}

QWidget *UiPages::createReportsPage(QWidget *parent)
{
    return new ReportsPage(parent);
}
