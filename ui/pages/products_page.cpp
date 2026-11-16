#include "products_page.h"

#include "../../POS/product/product.h"
#include "../../POS/product/product_repository.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QVector>

namespace
{
const QString kNavy = QStringLiteral("#111184");
const QString kInk = QStringLiteral("#20233B");
const QString kMuted = QStringLiteral("#777B91");
const QString kLine = QStringLiteral("#E7E9F3");

QString productFilePath()
{
    const QString workingDirectoryPath = QDir::current().filePath(
        QStringLiteral("Data/Product.txt"));
    if (QFileInfo::exists(workingDirectoryPath))
        return workingDirectoryPath;
    return QDir(QCoreApplication::applicationDirPath()).filePath(
        QStringLiteral("../../Data/Product.txt"));
}

QString barcodeImagePath(const QString &imagePath)
{
    if (QFileInfo::exists(imagePath))
        return imagePath;
    return QDir(QCoreApplication::applicationDirPath()).filePath(
        QStringLiteral("../../%1").arg(imagePath));
}

QFrame *createCard(QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("productsCard"));
    card->setStyleSheet(QStringLiteral(
        "QFrame#productsCard { background: white; border: 1px solid %1; border-radius: 14px; }")
        .arg(kLine));
    return card;
}

QFrame *createStatCard(QWidget *parent, const QString &title, const QString &accent,
    QLabel **value, const QString &hint)
{
    auto *card = createCard(parent);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(17, 15, 17, 15);
    layout->setSpacing(4);
    auto *heading = new QLabel(title, card);
    heading->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 10px; font-weight: 600; background: transparent;").arg(kMuted));
    *value = new QLabel(QStringLiteral("0"), card);
    (*value)->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 24px; font-weight: 700; background: transparent;").arg(kInk));
    auto *subheading = new QLabel(hint, card);
    subheading->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 9px; font-weight: 600; background: transparent;").arg(accent));
    layout->addWidget(heading);
    layout->addWidget(*value);
    layout->addWidget(subheading);
    return card;
}

class ProductsPage : public QWidget
{
public:
    explicit ProductsPage(QWidget *parent) : QWidget(parent)
    {
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 18);
        root->setSpacing(16);

        auto *stats = new QHBoxLayout;
        stats->setSpacing(12);
        stats->addWidget(createStatCard(this, QStringLiteral("TOTAL PRODUCTS"),
            QStringLiteral("#4F46E5"), &productCount_, QStringLiteral("In your product list")), 1);
        stats->addWidget(createStatCard(this, QStringLiteral("LOW STOCK"),
            QStringLiteral("#D97706"), &lowStockCount_, QStringLiteral("10 units or less")), 1);
        stats->addWidget(createStatCard(this, QStringLiteral("TOTAL UNITS"),
            QStringLiteral("#16845B"), &unitCount_, QStringLiteral("Across all products")), 1);
        root->addLayout(stats);

        auto *card = createCard(this);
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(18, 18, 18, 16);
        layout->setSpacing(13);

        auto *heading = new QHBoxLayout;
        auto *title = new QLabel(QStringLiteral("Product inventory"), card);
        title->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 15px; font-weight: 700; background: transparent;").arg(kInk));
        heading->addWidget(title);
        heading->addStretch(1);
        countLabel_ = new QLabel(card);
        countLabel_->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; background: transparent;").arg(kMuted));
        heading->addWidget(countLabel_);
        layout->addLayout(heading);

        auto *filters = new QHBoxLayout;
        search_ = new QLineEdit(card);
        search_->setPlaceholderText(QStringLiteral("Search by product, barcode or code"));
        search_->setFixedHeight(40);
        search_->setStyleSheet(QStringLiteral(
            "QLineEdit { background: #FAFAFD; color: %1; border: 1px solid %2;"
            " border-radius: 8px; padding: 0 12px; font-size: 11px; }"
            "QLineEdit:focus { background: white; border: 1px solid %3; }")
            .arg(kInk, kLine, kNavy));
        filters->addWidget(search_, 1);

        lowStockOnly_ = new QCheckBox(QStringLiteral("Low stock only"), card);
        lowStockOnly_->setCursor(Qt::PointingHandCursor);
        lowStockOnly_->setStyleSheet(QStringLiteral(
            "QCheckBox { color: %1; font-size: 10px; font-weight: 600; spacing: 7px; }"
            "QCheckBox::indicator { width: 16px; height: 16px; }"
            "QCheckBox::indicator:unchecked { background: white; border: 1px solid %2;"
            " border-radius: 4px; }"
            "QCheckBox::indicator:checked { background: %3; border: 1px solid %3;"
            " border-radius: 4px; }").arg(kMuted, kLine, kNavy));
        filters->addWidget(lowStockOnly_);
        layout->addLayout(filters);

        table_ = new QTableWidget(card);
        table_->setColumnCount(7);
        table_->setHorizontalHeaderLabels({
            QStringLiteral("PRODUCT"), QStringLiteral("BARCODE"),
            QStringLiteral("UNIT"), QStringLiteral("PRICE"),
            QStringLiteral("TAX"), QStringLiteral("DISCOUNT"),
            QStringLiteral("AVAILABLE STOCK")
        });
        table_->verticalHeader()->hide();
        table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        for (int column = 3; column < 7; ++column)
            table_->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
        table_->horizontalHeader()->setFixedHeight(40);
        table_->setShowGrid(false);
        table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table_->setSelectionBehavior(QAbstractItemView::SelectRows);
        table_->setSelectionMode(QAbstractItemView::SingleSelection);
        table_->setStyleSheet(QStringLiteral(
            "QTableWidget { background: white; border: none; color: %1; }"
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
        root->addWidget(card, 1);

        note_ = new QLabel(this);
        note_->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; background: transparent;").arg(kMuted));
        root->addWidget(note_);

        connect(search_, &QLineEdit::textChanged, this, [this]() { applyFilters(); });
        connect(lowStockOnly_, &QCheckBox::toggled, this, [this]() { applyFilters(); });
        loadProducts();
    }

private:
    void loadProducts()
    {
        QString error;
        if (!Pos::readProducts(productFilePath(), products_, error))
        {
            note_->setText(error);
            note_->setStyleSheet(QStringLiteral(
                "color: #B42318; font-size: 10px; background: transparent;"));
            table_->hide();
            emptyLabel_->setText(QStringLiteral("Products could not be loaded."));
            emptyLabel_->show();
            return;
        }

        int lowStockCount = 0;
        int unitCount = 0;
        for (const Pos::Product &product : products_)
        {
            if (product.quantity <= 10)
                ++lowStockCount;
            unitCount += product.quantity;
        }
        productCount_->setText(QString::number(products_.size()));
        lowStockCount_->setText(QString::number(lowStockCount));
        unitCount_->setText(QString::number(unitCount));
        note_->setText(QStringLiteral(
            "Product data is loaded from Data/Product.txt. Stock of 10 units or less is marked low."));
        applyFilters();
    }

    void applyFilters()
    {
        if (table_->isHidden() && !products_.isEmpty())
            return;

        const QString query = search_->text().trimmed().toLower();
        const bool lowStockOnly = lowStockOnly_->isChecked();
        table_->setRowCount(products_.size());
        int visibleCount = 0;

        for (int row = 0; row < products_.size(); ++row)
        {
            const Pos::Product &product = products_.at(row);
            QString name = product.name;
            name.replace(QLatin1Char('_'), QLatin1Char(' '));
            const QString searchableText = QStringLiteral("%1 %2 %3 %4")
                .arg(name, product.id, product.barcode, product.acronym).toLower();
            const bool isLowStock = product.quantity <= 10;
            const bool visible = (query.isEmpty() || searchableText.contains(query)) &&
                (!lowStockOnly || isLowStock);
            table_->setRowHidden(row, !visible);
            if (!visible)
                continue;
            ++visibleCount;

            auto *productCell = new QWidget(table_);
            auto *productLayout = new QHBoxLayout(productCell);
            productLayout->setContentsMargins(6, 4, 6, 4);
            productLayout->setSpacing(10);
            auto *image = new QLabel(productCell);
            image->setFixedSize(72, 38);
            image->setAlignment(Qt::AlignCenter);
            image->setStyleSheet(QStringLiteral("background: #F8F8FC; border-radius: 5px;"));
            const QPixmap barcode(barcodeImagePath(product.barcodeImage));
            if (!barcode.isNull())
                image->setPixmap(barcode.scaled(image->size(), Qt::KeepAspectRatio,
                    Qt::SmoothTransformation));
            auto *nameLabel = new QLabel(productCell);
            nameLabel->setText(QStringLiteral(
                "<b>%1</b><br><span style='color:%2;font-size:9px;'>%3 · %4</span>")
                .arg(name.toHtmlEscaped(), kMuted, product.id.toHtmlEscaped(),
                    product.acronym.toHtmlEscaped()));
            nameLabel->setStyleSheet(QStringLiteral(
                "color: %1; font-size: 11px; background: transparent;").arg(kInk));
            productLayout->addWidget(image);
            productLayout->addWidget(nameLabel, 1);
            table_->setCellWidget(row, 0, productCell);

            setCell(row, 1, product.barcode);
            setCell(row, 2, product.unit);
            setCell(row, 3, QStringLiteral("PKR %1").arg(product.unitPrice, 0, 'f', 2));
            setCell(row, 4, QStringLiteral("PKR %1").arg(product.tax, 0, 'f', 2));
            setCell(row, 5, QStringLiteral("PKR %1").arg(product.discountPerUnit, 0, 'f', 2));

            auto *stock = new QTableWidgetItem(QStringLiteral("%1 %2")
                .arg(product.quantity).arg(product.unit));
            stock->setFlags(stock->flags() & ~Qt::ItemIsEditable);
            stock->setTextAlignment(Qt::AlignCenter);
            stock->setForeground(QColor(isLowStock
                ? QStringLiteral("#B7791F") : QStringLiteral("#18764A")));
            table_->setItem(row, 6, stock);
            table_->setRowHeight(row, 58);
        }

        const bool hasVisibleProducts = visibleCount > 0;
        table_->setVisible(hasVisibleProducts);
        emptyLabel_->setVisible(!hasVisibleProducts);
        if (products_.isEmpty())
            emptyLabel_->setText(QStringLiteral("No products have been added to Product.txt yet."));
        else if (lowStockOnly && visibleCount == 0)
            emptyLabel_->setText(QStringLiteral("No low-stock products found."));
        else
            emptyLabel_->setText(QStringLiteral("No products match your search."));
        countLabel_->setText(QStringLiteral("Showing %1 of %2 products")
            .arg(visibleCount).arg(products_.size()));
    }

    void setCell(int row, int column, const QString &text)
    {
        auto *cell = new QTableWidgetItem(text);
        cell->setFlags(cell->flags() & ~Qt::ItemIsEditable);
        table_->setItem(row, column, cell);
    }

    QVector<Pos::Product> products_;
    QLabel *productCount_ = nullptr;
    QLabel *lowStockCount_ = nullptr;
    QLabel *unitCount_ = nullptr;
    QLabel *countLabel_ = nullptr;
    QLabel *emptyLabel_ = nullptr;
    QLabel *note_ = nullptr;
    QLineEdit *search_ = nullptr;
    QCheckBox *lowStockOnly_ = nullptr;
    QTableWidget *table_ = nullptr;
};
}

QWidget *UiPages::createProductsPage(QWidget *parent)
{
    return new ProductsPage(parent);
}
