#include "products_page.h"
#include "../theme.h"

#include "../../POS/category/category_repository.h"
#include "../../POS/product/product.h"
#include "../../POS/product/product_repository.h"
#include "../../POS/product/product_service.h"
#include "product_dialog.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
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

QString orderDetailFilePath()
{
    // OrderDetail.txt sits next to Product.txt in the Data folder.
    return QFileInfo(productFilePath()).dir().filePath(QStringLiteral("OrderDetail.txt"));
}

QString categoryFilePath()
{
    // PCategory.txt sits next to Product.txt in the Data folder.
    return QFileInfo(productFilePath()).dir().filePath(QStringLiteral("PCategory.txt"));
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
        heading->addSpacing(10);

        editButton_ = new QPushButton(QStringLiteral("Edit selected"), card);
        editButton_->setCursor(Qt::PointingHandCursor);
        editButton_->setFixedHeight(36);
        editButton_->setEnabled(false);
        editButton_->setToolTip(QStringLiteral("Change price, discount or stock (or double-click a row)"));
        editButton_->setStyleSheet(QStringLiteral(
            "QPushButton { background: #F0F0FF; color: %1; border: 1px solid #E1E2FA;"
            " border-radius: 8px; padding: 0 14px; font-size: 10px; font-weight: 600; }"
            "QPushButton:hover { background: #E8E9FF; }"
            "QPushButton:disabled { color: #A9ABBD; background: #F6F6FA; border-color: %2; }")
            .arg(kNavy, kLine));
        heading->addWidget(editButton_);

        deleteButton_ = new QPushButton(QStringLiteral("Delete"), card);
        deleteButton_->setCursor(Qt::PointingHandCursor);
        deleteButton_->setFixedHeight(36);
        deleteButton_->setEnabled(false);
        deleteButton_->setToolTip(QStringLiteral("Delete a product that has never been sold"));
        deleteButton_->setStyleSheet(QStringLiteral(
            "QPushButton { background: #FEF3F2; color: #B42318; border: 1px solid #FECDCA;"
            " border-radius: 8px; padding: 0 14px; font-size: 10px; font-weight: 600; }"
            "QPushButton:hover { background: #FEE4E2; }"
            "QPushButton:disabled { color: #A9ABBD; background: #F6F6FA; border-color: %1; }")
            .arg(kLine));
        heading->addWidget(deleteButton_);

        auto *addButton = new QPushButton(QStringLiteral("+ Add product"), card);
        addButton->setCursor(Qt::PointingHandCursor);
        addButton->setFixedHeight(36);
        addButton->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; color: white; border: none; border-radius: 8px;"
            " padding: 0 14px; font-size: 10px; font-weight: 700; }"
            "QPushButton:hover { background: #0D0D6B; }").arg(kNavy));
        heading->addWidget(addButton);
        layout->addLayout(heading);

        auto *filters = new QHBoxLayout;
        search_ = new QLineEdit(card);
        search_->setPlaceholderText(QStringLiteral("Search by name, barcode, ID or acronym"));
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
            " border-radius: 4px; image: url(:/assets/checkbox_check.png); }")
            .arg(kMuted, kLine, kNavy));
        filters->addWidget(lowStockOnly_);
        layout->addLayout(filters);

        table_ = new QTableWidget(card);
        table_->setColumnCount(8);
        table_->setHorizontalHeaderLabels({
            QStringLiteral("PRODUCT"), QStringLiteral("BARCODE"),
            QStringLiteral("CATEGORY"), QStringLiteral("UNIT"), QStringLiteral("PRICE"),
            QStringLiteral("TAX"), QStringLiteral("DISCOUNT"),
            QStringLiteral("AVAILABLE STOCK")
        });
        table_->verticalHeader()->hide();
        table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        for (int column = 1; column < 8; ++column)
            table_->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
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
        root->addWidget(card, 1);

        note_ = new QLabel(this);
        note_->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; background: transparent;").arg(kMuted));
        root->addWidget(note_);

        connect(search_, &QLineEdit::textChanged, this, [this]() { applyFilters(); });
        connect(lowStockOnly_, &QCheckBox::toggled, this, [this]() { applyFilters(); });
        connect(table_, &QTableWidget::itemSelectionChanged, this, [this]() {
            editButton_->setEnabled(selectedProductRow() >= 0);
            deleteButton_->setEnabled(selectedProductRow() >= 0);
        });
        connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int, int) { editSelected(); });
        connect(editButton_, &QPushButton::clicked, this, [this]() { editSelected(); });
        connect(deleteButton_, &QPushButton::clicked, this, [this]() { deleteSelected(); });
        connect(addButton, &QPushButton::clicked, this, [this]() {
            if (UiPages::showAddProductDialog(this, productFilePath()))
            {
                loadProducts();
                showSaved(QStringLiteral("Product added to Product.txt."));
            }
        });
        loadProducts();
    }

private:
    void loadProducts()
    {
        QString error;
        loadFailed_ = !Pos::readProducts(productFilePath(), products_, error);
        if (loadFailed_)
        {
            note_->setText(error);
            note_->setStyleSheet(QStringLiteral(
                "color: #B42318; font-size: 10px; background: transparent;"));
            table_->hide();
            emptyLabel_->setText(QStringLiteral("Products could not be loaded."));
            emptyLabel_->show();
            return;
        }

        // Category names for the CATEGORY column (PCategory.txt).
        categoryNames_.clear();
        QVector<Pos::Category> categories;
        QString categoryError;
        if (Pos::readCategories(categoryFilePath(), categories, categoryError))
            for (const Pos::Category &category : categories)
                categoryNames_.insert(category.id, QString(category.name).replace(QLatin1Char('_'), QLatin1Char(' ')));

        int unitCount = 0;
        for (const Pos::Product &product : products_)
            unitCount += product.quantity;
        productCount_->setText(QString::number(products_.size()));
        lowStockCount_->setText(QString::number(
            Pos::lowStockProducts(products_, kLowStockThreshold).size()));
        unitCount_->setText(QString::number(unitCount));
        note_->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; background: transparent;").arg(kMuted));
        note_->setText(QStringLiteral(
            "Product data is loaded from Data/Product.txt. Stock of 10 units or less is marked low."));
        table_->show();
        applyFilters();
    }

    void applyFilters()
    {
        if (loadFailed_)
            return;

        // Search and low-stock rules live in product_service, not in the UI.
        const bool lowStockOnly = lowStockOnly_->isChecked();
        visible_ = Pos::searchProducts(products_, search_->text());
        if (lowStockOnly)
            visible_ = Pos::lowStockProducts(visible_, kLowStockThreshold);

        table_->clearSelection();
        table_->setRowCount(0);   // drop the old rows and their cell widgets first
        table_->setRowCount(visible_.size());
        const int visibleCount = visible_.size();

        for (int row = 0; row < visible_.size(); ++row)
        {
            const Pos::Product &product = visible_.at(row);
            QString name = product.name;
            name.replace(QLatin1Char('_'), QLatin1Char(' '));
            const bool isLowStock = product.quantity <= kLowStockThreshold;

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
            const QString categoryId = product.categoryId;   // stored 11th field (req 2)
            auto *category = new QTableWidgetItem(categoryNames_.value(categoryId,
                QStringLiteral("Not found")));
            category->setFlags(category->flags() & ~Qt::ItemIsEditable);
            if (!categoryNames_.contains(categoryId))
            {
                category->setForeground(UiTheme::textColor(QColor(QStringLiteral("#B42318"))));
                category->setToolTip(QStringLiteral(
                    "No category %1 in PCategory.txt. Add it on the Categories page.").arg(categoryId));
            }
            table_->setItem(row, 2, category);
            setCell(row, 3, product.unit);
            setCell(row, 4, QStringLiteral("PKR %1").arg(product.unitPrice, 0, 'f', 2));
            setCell(row, 5, QStringLiteral("PKR %1").arg(product.tax, 0, 'f', 2));
            setCell(row, 6, QStringLiteral("PKR %1").arg(product.discountPerUnit, 0, 'f', 2));

            auto *stock = new QTableWidgetItem(QStringLiteral("%1 %2")
                .arg(product.quantity).arg(product.unit));
            stock->setFlags(stock->flags() & ~Qt::ItemIsEditable);
            stock->setTextAlignment(Qt::AlignCenter);
            stock->setForeground(UiTheme::textColor(QColor(isLowStock
                ? QStringLiteral("#B7791F") : QStringLiteral("#18764A"))));
            table_->setItem(row, 7, stock);
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

    int selectedProductRow() const
    {
        const QModelIndexList rows = table_->selectionModel()
            ? table_->selectionModel()->selectedRows() : QModelIndexList();
        if (rows.isEmpty() || rows.first().row() >= visible_.size())
            return -1;
        return rows.first().row();
    }

    void editSelected()
    {
        const int row = selectedProductRow();
        if (row < 0)
            return;
        const Pos::Product product = visible_.at(row);
        if (UiPages::showEditProductDialog(this, productFilePath(), product))
        {
            loadProducts();
            showSaved(QStringLiteral("Product %1 updated in Product.txt.").arg(product.id));
        }
    }

    void deleteSelected()
    {
        const int row = selectedProductRow();
        if (row < 0)
            return;
        const Pos::Product product = visible_.at(row);
        QString name = product.name;
        name.replace(QLatin1Char('_'), QLatin1Char(' '));
        const auto answer = QMessageBox::question(this, QStringLiteral("Delete product"),
            QStringLiteral("Delete %1 (%2) from Product.txt?\nThis cannot be undone.")
                .arg(name, product.id),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;

        QString error;
        if (!Pos::deleteProduct(productFilePath(), orderDetailFilePath(), product.id, error))
        {
            QMessageBox::warning(this, QStringLiteral("Cannot delete product"), error);
            return;
        }
        loadProducts();
        showSaved(QStringLiteral("Product %1 deleted from Product.txt.").arg(product.id));
    }

    void showSaved(const QString &message)
    {
        note_->setStyleSheet(QStringLiteral(
            "color: #18764A; font-size: 10px; font-weight: 600; background: transparent;"));
        note_->setText(message);
    }

    void setCell(int row, int column, const QString &text)
    {
        auto *cell = new QTableWidgetItem(text);
        cell->setFlags(cell->flags() & ~Qt::ItemIsEditable);
        table_->setItem(row, column, cell);
    }

    static constexpr int kLowStockThreshold = 10;

    QVector<Pos::Product> products_;
    QVector<Pos::Product> visible_;
    QHash<QString, QString> categoryNames_;   // Category Id -> name
    bool loadFailed_ = false;
    QPushButton *editButton_ = nullptr;
    QPushButton *deleteButton_ = nullptr;
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
