#include "reports_page.h"

#include "../../POS/category/category_repository.h"
#include "../../POS/common/data_paths.h"
#include "../../POS/product/product_service.h"
#include "../../POS/report/report_service.h"

#include <QComboBox>
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

struct ReportDefinition
{
    QString title;
    QString description;
    // unit: "" for all items, "count" or "weight" for the unit filter.
    std::function<bool(ReportTable &table, QString &error, const QString &unit)> build;
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

// ---------------------------------------------------------------------------
// The reports. To add one (e.g. cash sales), put its calculation in
// POS/report/ and add one entry to this list; the page does the rest.
// ---------------------------------------------------------------------------
QVector<ReportDefinition> reports()
{
    return {
        {QStringLiteral("Top 10 most sold items"),
         QStringLiteral("Items sold in paid orders, most sold first, with the stock that is left. "
                        "Held and archived orders are not counted."),
         [](ReportTable &table, QString &error, const QString &unit) {
             // Owner: Umar Khan
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
         [](ReportTable &table, QString &error, const QString &unit) {
             // Owner: Umar Khan
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
        layout->addWidget(emptyLabel_);

        footer_ = new QLabel(card);
        footer_->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; font-weight: 600; background: transparent;").arg(kInk));
        layout->addWidget(footer_);
        root->addWidget(card, 1);

        connect(selector_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { refreshReport(); });
        connect(unitFilter_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { refreshReport(); });
        refreshReport();
    }

private:
    void refreshReport()
    {
        const int index = selector_->currentIndex();
        if (index < 0 || index >= reports_.size())
            return;
        const ReportDefinition &report = reports_.at(index);
        description_->setText(report.description);

        ReportTable result;
        QString error;
        const bool ok = report.build(result, error, unitFilter_->currentData().toString());
        table_->clear();
        table_->setRowCount(0);
        if (!ok)
        {
            table_->hide();
            emptyLabel_->setText(QStringLiteral("The report could not be made: %1").arg(error));
            emptyLabel_->setStyleSheet(emptyLabel_->styleSheet().replace(kMuted, QStringLiteral("#B42318")));
            emptyLabel_->show();
            footer_->clear();
            return;
        }
        emptyLabel_->setStyleSheet(emptyLabel_->styleSheet().replace(QStringLiteral("#B42318"), kMuted));

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
