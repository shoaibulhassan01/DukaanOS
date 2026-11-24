#include "product_dialog.h"
#include "categories_page.h"

#include "../../POS/category/category_service.h"
#include "../../POS/product/product_service.h"
#include "../../POS/product/product_category.h"
#include "../../POS/product/barcode_image.h"

#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QDoubleValidator>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QIntValidator>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QScrollArea>
#include <QScreen>
#include <QVBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QMessageBox>
#include <QPixmap>
#include <QRegularExpression>

namespace
{
const QString kNavy = QStringLiteral("#111184");
const QString kInk = QStringLiteral("#20233B");
const QString kMuted = QStringLiteral("#777B91");
const QString kLine = QStringLiteral("#E7E9F3");

const QString kInputStyle = QStringLiteral(
    "QLineEdit, QComboBox { background: #FAFAFD; color: %1;"
    " border: 1px solid %2; border-radius: 9px; padding: 0 11px; min-height: 40px;"
    " font-size: 12px; }"
    "QLineEdit:focus, QComboBox:focus"
    " { background: white; border: 1px solid %3; }"
    "QLineEdit:disabled, QComboBox:disabled { color: %4; background: #F1F2F7; }"
    // The arrow sits inside the field: no separate box or divider around it.
    "QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: center right;"
    " width: 28px; border: none; background: transparent; }"
    "QComboBox::down-arrow { image: url(:/assets/combo_arrow.png); width: 12px; height: 8px; }")
    .arg(kInk, kLine, kNavy, kMuted);

enum class Mode { Add, Edit };

class ProductDialog : public QDialog
{
public:
    ProductDialog(QWidget *parent, const QString &productFile, Mode mode,
        const Pos::Product &product)
        : QDialog(parent), productFile_(productFile), mode_(mode), original_(product),
          categoryFile_(QFileInfo(productFile).absoluteDir().filePath(QStringLiteral("PCategory.txt")))
    {
        setWindowTitle(mode == Mode::Add ? QStringLiteral("Add product")
                                         : QStringLiteral("Edit product"));
        setModal(true);
        setMinimumSize(820, 600);
        setStyleSheet(QStringLiteral("QDialog { background: white; }") + kInputStyle);

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(28, 24, 28, 22);
        root->setSpacing(16);

        auto *title = new QLabel(windowTitle(), this);
        title->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 20px; font-weight: 700;").arg(kInk));
        root->addWidget(title);

        auto *hint = new QLabel(mode == Mode::Add
            ? QStringLiteral("The product ID is assigned automatically. Enter a 6-digit barcode whose first two digits match a category. The barcode picture is created for you.")
            : QStringLiteral("Only the price, discount and available stock can be changed."), this);
        hint->setWordWrap(true);
        hint->setStyleSheet(QStringLiteral("color: %1; font-size: 10px;").arg(kMuted));
        root->addWidget(hint);

        auto *form = new QFormLayout;
        form->setHorizontalSpacing(18);
        form->setVerticalSpacing(12);
        form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        id_ = new QLineEdit(this);
        id_->setToolTip(QStringLiteral("Assigned automatically: one more than the highest product id"));
        barcode_ = new QLineEdit(this);
        barcode_->setPlaceholderText(QStringLiteral("e.g. 100004"));
        barcode_->setMaxLength(6);
        barcode_->setValidator(new QRegularExpressionValidator(
            QRegularExpression(QStringLiteral("\\d{0,6}")), barcode_));
        categoryInfo_ = new QLabel(this);
        categoryInfo_->setWordWrap(true);
        // Shortcut when the barcode's category is missing: opens the same
        // Add category form as the Categories page, with the Id locked.
        createCategory_ = new QPushButton(this);
        createCategory_->setCursor(Qt::PointingHandCursor);
        createCategory_->setFlat(true);
        createCategory_->setStyleSheet(QStringLiteral(
            "QPushButton { color: %1; background: transparent; border: none; padding: 0;"
            " font-size: 10px; font-weight: 700; text-align: left; }"
            "QPushButton:hover { text-decoration: underline; }").arg(kNavy));
        createCategory_->hide();
        auto *categoryRow = new QWidget(this);
        auto *categoryLayout = new QVBoxLayout(categoryRow);
        categoryLayout->setContentsMargins(0, 0, 0, 0);
        categoryLayout->setSpacing(3);
        categoryLayout->addWidget(categoryInfo_);
        categoryLayout->addWidget(createCategory_, 0, Qt::AlignLeft);
        acronym_ = new QLineEdit(this);
        acronym_->setPlaceholderText(QStringLiteral("Up to 8 characters"));
        acronym_->setMaxLength(8);
        name_ = new QLineEdit(this);
        name_->setPlaceholderText(QStringLiteral("e.g. Green tea"));
        unit_ = new QComboBox(this);
        unit_->setObjectName(QStringLiteral("productUnit"));
        // Shown as "weight (kg)", saved as "weight" in Product.txt.
        unit_->addItem(Pos::unitLabel(QStringLiteral("count")), QStringLiteral("count"));
        unit_->addItem(Pos::unitLabel(QStringLiteral("weight")), QStringLiteral("weight"));
        price_ = moneyBox();
        tax_ = moneyBox();
        discount_ = moneyBox();
        quantity_ = new QLineEdit(this);
        quantity_->setPlaceholderText(QStringLiteral("e.g. 10"));
        quantity_->setValidator(new QIntValidator(0, 1000000, quantity_));
        image_ = new QLineEdit(this);
        image_->setPlaceholderText(QStringLiteral("Created from the barcode"));
        image_->setToolTip(QStringLiteral("The barcode picture is generated automatically when the product is added"));

        form->addRow(label(QStringLiteral("Product id")), id_);
        form->addRow(label(QStringLiteral("Barcode")), barcode_);
        form->addRow(label(QStringLiteral("Category")), categoryRow);
        form->addRow(label(QStringLiteral("Name")), name_);
        form->addRow(label(QStringLiteral("Acronym")), acronym_);
        form->addRow(label(QStringLiteral("Unit")), unit_);
        form->addRow(label(QStringLiteral("Unit price (PKR)")), price_);
        form->addRow(label(QStringLiteral("Tax per unit (PKR)")), tax_);
        form->addRow(label(QStringLiteral("Discount per unit (PKR)")), discount_);
        form->addRow(label(QStringLiteral("Available quantity")), quantity_);
        form->addRow(label(QStringLiteral("Barcode image")), image_);

        auto *formContent = new QWidget(this);
        formContent->setLayout(form);
        formContent->setMinimumWidth(440);
        auto *formScroll = new QScrollArea(this);
        formScroll->setWidgetResizable(true);
        formScroll->setFrameShape(QFrame::NoFrame);
        formScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        formScroll->setWidget(formContent);

        auto *content = new QHBoxLayout;
        content->setSpacing(18);
        content->addWidget(formScroll, 3);

        barcodeCard_ = new QFrame(this);
        barcodeCard_->setObjectName(QStringLiteral("barcodeCard"));
        barcodeCard_->setMinimumWidth(290);
        barcodeCard_->setMaximumWidth(360);
        barcodeCard_->setStyleSheet(QStringLiteral(
            "QFrame#barcodeCard { background: #F8F9FF; border: 1px solid %1; border-radius: 14px; }")
            .arg(kLine));
        auto *barcodeLayout = new QVBoxLayout(barcodeCard_);
        barcodeLayout->setContentsMargins(18, 18, 18, 18);
        barcodeLayout->setSpacing(12);
        auto *barcodeTitle = new QLabel(QStringLiteral("Barcode preview"), barcodeCard_);
        barcodeTitle->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 14px; font-weight: 700; background: transparent;").arg(kInk));
        barcodeLayout->addWidget(barcodeTitle);
        auto *barcodeHint = new QLabel(
            QStringLiteral("Your product name and barcode will appear on the saved image."),
            barcodeCard_);
        barcodeHint->setWordWrap(true);
        barcodeHint->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 11px; background: transparent;").arg(kMuted));
        barcodeLayout->addWidget(barcodeHint);
        barcodePreview_ = new QLabel(QStringLiteral("Enter a product name and 6-digit barcode."),
            barcodeCard_);
        barcodePreview_->setObjectName(QStringLiteral("barcodePreview"));
        barcodePreview_->setAlignment(Qt::AlignCenter);
        barcodePreview_->setMinimumHeight(190);
        barcodePreview_->setWordWrap(true);
        barcodePreview_->setStyleSheet(QStringLiteral(
            "background: white; border: 1px dashed %1; border-radius: 10px;"
            " color: %2; font-size: 11px; padding: 8px;").arg(kLine, kMuted));
        barcodeLayout->addWidget(barcodePreview_, 1);
        barcodeFileLabel_ = new QLabel(QStringLiteral("PNG image · ready after name and barcode"),
            barcodeCard_);
        barcodeFileLabel_->setWordWrap(true);
        barcodeFileLabel_->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; background: transparent;").arg(kMuted));
        barcodeLayout->addWidget(barcodeFileLabel_);
        downloadBarcodeButton_ = new QPushButton(QStringLiteral("Download barcode"), barcodeCard_);
        downloadBarcodeButton_->setCursor(Qt::PointingHandCursor);
        downloadBarcodeButton_->setFixedHeight(42);
        downloadBarcodeButton_->setEnabled(false);
        downloadBarcodeButton_->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; color: white; border: none; border-radius: 9px;"
            " padding: 0 16px; font-size: 12px; font-weight: 700; }"
            "QPushButton:hover { background: #0D0D6B; }"
            "QPushButton:disabled { background: #B9BADD; color: white; }").arg(kNavy));
        barcodeLayout->addWidget(downloadBarcodeButton_);
        content->addWidget(barcodeCard_, 2);
        root->addLayout(content, 1);

        error_ = new QLabel(this);
        error_->setWordWrap(true);
        error_->setStyleSheet(QStringLiteral(
            "color: #B42318; background: #FEF3F2; border: 1px solid #FECDCA;"
            " border-radius: 7px; padding: 7px 9px; font-size: 10px;"));
        error_->hide();
        root->addWidget(error_);

        auto *buttons = new QHBoxLayout;
        buttons->addStretch(1);
        auto *cancel = new QPushButton(QStringLiteral("Cancel"), this);
        cancel->setCursor(Qt::PointingHandCursor);
        cancel->setFixedHeight(38);
        cancel->setStyleSheet(QStringLiteral(
            "QPushButton { background: white; color: %1; border: 1px solid %2;"
            " border-radius: 8px; padding: 0 16px; font-size: 11px; font-weight: 600; }"
            "QPushButton:hover { background: #F8F8FC; }").arg(kInk, kLine));
        auto *save = new QPushButton(mode == Mode::Add ? QStringLiteral("Add product")
                                                       : QStringLiteral("Save changes"), this);
        save->setCursor(Qt::PointingHandCursor);
        save->setFixedHeight(38);
        save->setDefault(true);
        save->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; color: white; border: none; border-radius: 8px;"
            " padding: 0 18px; font-size: 11px; font-weight: 700; }"
            "QPushButton:hover { background: #0D0D6B; }").arg(kNavy));
        buttons->addWidget(cancel);
        buttons->addWidget(save);
        root->addLayout(buttons);

        if (QScreen *screen = QGuiApplication::primaryScreen())
        {
            const int maxHeight = qMax(600, screen->availableGeometry().height() * 9 / 10);
            setMaximumHeight(maxHeight);
            resize(qMax(820, qMin(1040, screen->availableGeometry().width() * 9 / 10)),
                qMin(780, maxHeight));
        }

        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
        connect(save, &QPushButton::clicked, this, [this]() { trySave(); });
        connect(downloadBarcodeButton_, &QPushButton::clicked, this,
            [this]() { downloadBarcode(); });

        if (mode == Mode::Add)
        {
            image_->setReadOnly(true);
            image_->setPlaceholderText(QStringLiteral("Generated from product name and barcode"));
            QString nextId;
            QString idError;
            if (Pos::nextProductId(productFile_, nextId, idError))
                id_->setText(nextId);
            else
                id_->setPlaceholderText(idError);
            id_->setEnabled(false);
            connect(acronym_, &QLineEdit::textChanged, this, [this](const QString &text) {
                if (text != suggestedAcronym_)
                    acronymManuallyEdited_ = true;
            });
            connect(name_, &QLineEdit::textChanged, this, [this](const QString &text) {
                if (acronymManuallyEdited_)
                {
                    updateBarcodePreview();
                    return;
                }
                QString suggestion = text.simplified();
                suggestion.remove(QLatin1Char(' '));
                suggestedAcronym_ = suggestion.left(3).toUpper();
                acronym_->setText(suggestedAcronym_);
                updateBarcodePreview();
            });
            connect(barcode_, &QLineEdit::textChanged, this, [this](const QString &text) {
                showCategory(text);
                updateBarcodePreview();
            });
            connect(createCategory_, &QPushButton::clicked, this, [this]() {
                const QString categoryId = barcode_->text().trimmed().left(2);
                if (UiPages::showAddCategoryForBarcode(this, categoryFile_, categoryId))
                    showCategory(barcode_->text());
            });
            showCategory(QString());
        }
        else
        {
            fill(product);
            updateBarcodePreview();
            showStoredCategory(product.categoryId);
            for (QWidget *locked : { static_cast<QWidget *>(id_), static_cast<QWidget *>(barcode_),
                     static_cast<QWidget *>(acronym_), static_cast<QWidget *>(name_),
                     static_cast<QWidget *>(unit_), static_cast<QWidget *>(tax_),
                     static_cast<QWidget *>(image_) })
                locked->setEnabled(false);
            price_->setFocus();
        }
    }

private:
    void updateBarcodePreview()
    {
        const QString barcode = barcode_->text().trimmed();
        const QString productName = name_->text().simplified();
        currentBarcodeImage_ = {};
        downloadBarcodeButton_->setEnabled(false);
        if (barcode.size() != 6 || productName.isEmpty())
        {
            barcodePreview_->setPixmap(QPixmap());
            barcodePreview_->setText(QStringLiteral("Enter a product name and 6-digit barcode."));
            image_->clear();
            barcodeFileLabel_->setText(QStringLiteral("PNG image · ready after name and barcode"));
            return;
        }

        QString storedName = productName;
        storedName.replace(QLatin1Char(' '), QLatin1Char('_'));
        const QString fileName = Pos::barcodeImageFileName(barcode, storedName);
        if (mode_ == Mode::Add)
            image_->setText(QStringLiteral("Data/barcodes/%1").arg(fileName));
        barcodeFileLabel_->setText(mode_ == Mode::Add
            ? fileName : QStringLiteral("Download as %1").arg(fileName));
        QString error;
        currentBarcodeImage_ = Pos::renderBarcodeImage(barcode, storedName, error);
        if (currentBarcodeImage_.isNull())
        {
            barcodePreview_->setPixmap(QPixmap());
            barcodePreview_->setText(error);
            return;
        }
        barcodePreview_->setText(QString());
        barcodePreview_->setPixmap(QPixmap::fromImage(currentBarcodeImage_).scaled(
            barcodePreview_->size().expandedTo(QSize(250, 150)),
            Qt::KeepAspectRatio, Qt::SmoothTransformation));
        downloadBarcodeButton_->setEnabled(true);
    }

    void downloadBarcode()
    {
        if (currentBarcodeImage_.isNull())
            return;
        const QString defaultName = Pos::barcodeImageFileName(
            barcode_->text().trimmed(),
            name_->text().simplified().replace(QLatin1Char(' '), QLatin1Char('_')));
        const QString path = QFileDialog::getSaveFileName(this,
            QStringLiteral("Download barcode"), defaultName,
            QStringLiteral("PNG image (*.png)"));
        if (path.isEmpty())
            return;
        if (!currentBarcodeImage_.save(path, "PNG"))
        {
            QMessageBox::warning(this, QStringLiteral("Download failed"),
                QStringLiteral("Could not save the barcode image to %1.").arg(path));
            return;
        }
        QMessageBox::information(this, QStringLiteral("Barcode downloaded"),
            QStringLiteral("Barcode image saved to %1.").arg(path));
    }

    QLabel *label(const QString &text)
    {
        auto *l = new QLabel(text, this);
        l->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(kInk));
        return l;
    }

    // Edit form: the category stored for the product (11th field of
    // Product.txt, req 2).
    void showStoredCategory(const QString &categoryId)
    {
        Pos::Category category;
        QString error;
        const bool found = Pos::findCategoryById(categoryFile_, categoryId, category, error);
        QString name = category.name;
        QString acronym = category.acronym;
        categoryInfo_->setText(found
            ? QStringLiteral("%1 \u00B7 %2 (%3)").arg(category.id,
                  name.replace(QLatin1Char('_'), QLatin1Char(' ')),
                  acronym.replace(QLatin1Char('_'), QLatin1Char(' ')))
            : QStringLiteral("Category %1 not found in PCategory.txt.").arg(categoryId));
        categoryInfo_->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; font-weight: 600;").arg(found
                ? QStringLiteral("#18764A") : QStringLiteral("#B42318")));
    }

    // Add form: the category the barcode being typed belongs to (its first
    // two digits, req 1), or why there is none.
    void showCategory(const QString &barcodeText)
    {
        const QString barcode = barcodeText.trimmed();
        QString text;
        QString color = kMuted;
        bool missingCategory = false;
        if (barcode.startsWith(QLatin1Char('0')))
        {
            text = QStringLiteral("A barcode cannot start with 0.");
            color = QStringLiteral("#B42318");
        }
        else if (barcode.size() < 2)
            text = QStringLiteral("Type the barcode: its first two digits choose the category.");
        else
        {
            Pos::Category category;
            QString error;
            if (Pos::findCategoryForBarcode(categoryFile_, barcode, category, error))
            {
                QString name = category.name;
                QString acronym = category.acronym;
                text = QStringLiteral("%1 \u00B7 %2 (%3)").arg(category.id,
                    name.replace(QLatin1Char('_'), QLatin1Char(' ')),
                    acronym.replace(QLatin1Char('_'), QLatin1Char(' ')));
                color = QStringLiteral("#18764A");
            }
            else
            {
                missingCategory = error.startsWith(QStringLiteral("No category"));
                text = missingCategory
                    ? QStringLiteral("Category %1 not found.").arg(barcode.left(2))
                    : error;
                color = QStringLiteral("#B42318");
            }
        }
        categoryInfo_->setText(text);
        categoryInfo_->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; font-weight: 600;").arg(color));
        createCategory_->setText(QStringLiteral("+ Create category %1\u2026").arg(barcode.left(2)));
        createCategory_->setVisible(missingCategory);
    }

    QLineEdit *moneyBox()
    {
        auto *box = new QLineEdit(this);
        box->setPlaceholderText(QStringLiteral("e.g. 250.00"));
        auto *validator = new QDoubleValidator(0.0, 10000000.0, 2, box);
        validator->setNotation(QDoubleValidator::StandardNotation);
        validator->setLocale(QLocale::c());
        box->setValidator(validator);
        return box;
    }

    void fill(const Pos::Product &p)
    {
        id_->setText(p.id);
        barcode_->setText(p.barcode);
        acronym_->setText(p.acronym);
        QString name = p.name;
        name_->setText(name.replace(QLatin1Char('_'), QLatin1Char(' ')));
        unit_->setCurrentIndex(qMax(0, unit_->findData(p.unit)));
        price_->setText(QString::number(p.unitPrice, 'f', 2));
        tax_->setText(QString::number(p.tax, 'f', 2));
        discount_->setText(QString::number(p.discountPerUnit, 'f', 2));
        quantity_->setText(QString::number(p.quantity));
        image_->setText(p.barcodeImage);
    }

    bool productFromForm(Pos::Product &p, QString &error) const
    {
        p.id = id_->text().trimmed();
        p.barcode = barcode_->text().trimmed();
        p.categoryId = p.barcode.left(2);
        p.acronym = acronym_->text().trimmed();
        // Product.txt is space separated, so spaces in a name are stored as _.
        p.name = name_->text().simplified().replace(QLatin1Char(' '), QLatin1Char('_'));
        p.unit = unit_->currentData().toString();
        p.barcodeImage = image_->text().trimmed();

        const QString priceText = price_->text().trimmed();
        const QString taxText = tax_->text().trimmed();
        const QString discountText = discount_->text().trimmed();
        const QString quantityText = quantity_->text().trimmed();
        bool valid = true;
        if (!priceText.isEmpty())
            p.unitPrice = QLocale::c().toDouble(priceText, &valid);
        if (!valid)
        {
            error = QStringLiteral("Unit price must be a valid amount.");
            return false;
        }
        if (!taxText.isEmpty())
            p.tax = QLocale::c().toDouble(taxText, &valid);
        if (!valid)
        {
            error = QStringLiteral("Tax must be a valid amount.");
            return false;
        }
        if (!discountText.isEmpty())
            p.discountPerUnit = QLocale::c().toDouble(discountText, &valid);
        if (!valid)
        {
            error = QStringLiteral("Discount must be a valid amount.");
            return false;
        }
        if (!quantityText.isEmpty())
            p.quantity = quantityText.toInt(&valid);
        if (!valid)
        {
            error = QStringLiteral("Quantity must be a whole number.");
            return false;
        }
        error.clear();
        return true;
    }

    void trySave()
    {
        QString error;
        bool ok = false;
        if (mode_ == Mode::Add)
        {
            Pos::Product product;
            if (!productFromForm(product, error))
            {
                error_->setText(error);
                error_->show();
                return;
            }
            ok = Pos::addProduct(productFile_, product, error);
        }
        else
        {
            bool validPrice = false;
            bool validDiscount = false;
            bool validQuantity = false;
            const double price = QLocale::c().toDouble(price_->text().trimmed(), &validPrice);
            const double discount = QLocale::c().toDouble(
                discount_->text().trimmed(), &validDiscount);
            const int quantity = quantity_->text().trimmed().toInt(&validQuantity);
            if (!validPrice || !validDiscount || !validQuantity)
            {
                error_->setText(QStringLiteral("Enter valid price, discount and quantity values."));
                error_->show();
                return;
            }
            ok = Pos::updateProduct(productFile_, original_.id, price, discount, quantity, error);
        }
        if (ok)
        {
            accept();
            return;
        }
        error_->setText(error);
        error_->show();
    }

    QString productFile_;
    Mode mode_;
    Pos::Product original_;
    QString categoryFile_;
    QLabel *categoryInfo_ = nullptr;
    QPushButton *createCategory_ = nullptr;
    QFrame *barcodeCard_ = nullptr;
    QLabel *barcodePreview_ = nullptr;
    QLabel *barcodeFileLabel_ = nullptr;
    QPushButton *downloadBarcodeButton_ = nullptr;
    QImage currentBarcodeImage_;
    bool acronymManuallyEdited_ = false;
    QString suggestedAcronym_;
    QLineEdit *id_ = nullptr;
    QLineEdit *barcode_ = nullptr;
    QLineEdit *acronym_ = nullptr;
    QLineEdit *name_ = nullptr;
    QComboBox *unit_ = nullptr;
    QLineEdit *price_ = nullptr;
    QLineEdit *tax_ = nullptr;
    QLineEdit *discount_ = nullptr;
    QLineEdit *quantity_ = nullptr;
    QLineEdit *image_ = nullptr;
    QLabel *error_ = nullptr;
};
}

bool UiPages::showAddProductDialog(QWidget *parent, const QString &productFile)
{
    ProductDialog dialog(parent, productFile, Mode::Add, Pos::Product());
    return dialog.exec() == QDialog::Accepted;
}

bool UiPages::showEditProductDialog(QWidget *parent, const QString &productFile,
    const Pos::Product &product)
{
    ProductDialog dialog(parent, productFile, Mode::Edit, product);
    return dialog.exec() == QDialog::Accepted;
}

bool UiPages::showProductCategoryDialog(QWidget *parent, const QString &productFile)
{
    const QString categoryFile = QFileInfo(productFile).absoluteDir()
        .filePath(QStringLiteral("PCategory.txt"));
    QVector<Pos::ProductCategory> categories;
    QString error;
    if (!Pos::readProductCategories(categoryFile, categories, error))
    {
        QMessageBox::warning(parent, QStringLiteral("Cannot load categories"), error);
        return false;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Configure product categories"));
    dialog.setModal(true);
    dialog.setMinimumWidth(520);
    auto *root = new QVBoxLayout(&dialog);
    root->setContentsMargins(20, 18, 20, 16);
    root->setSpacing(10);

    auto *hint = new QLabel(QStringLiteral(
        "Category IDs are exactly two digits and become the first two barcode digits. "
        "Use underscores instead of spaces."), &dialog);
    hint->setWordWrap(true);
    root->addWidget(hint);

    auto *table = new QTableWidget(&dialog);
    table->setColumnCount(3);
    table->setHorizontalHeaderLabels({QStringLiteral("CATEGORY ID"),
        QStringLiteral("CATEGORY NAME"), QStringLiteral("ACRONYM")});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->hide();
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    root->addWidget(table, 1);

    auto appendCategory = [table](const Pos::ProductCategory &category) {
        const int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 0, new QTableWidgetItem(category.id));
        table->setItem(row, 1, new QTableWidgetItem(category.name));
        table->setItem(row, 2, new QTableWidgetItem(category.acronym));
    };
    for (const Pos::ProductCategory &category : categories)
        appendCategory(category);

    auto *fields = new QHBoxLayout;
    auto *id = new QLineEdit(&dialog);
    id->setMaxLength(2);
    id->setPlaceholderText(QStringLiteral("ID (00-99)"));
    auto *name = new QLineEdit(&dialog);
    name->setPlaceholderText(QStringLiteral("Name"));
    auto *acronym = new QLineEdit(&dialog);
    acronym->setPlaceholderText(QStringLiteral("Acronym"));
    fields->addWidget(id);
    fields->addWidget(name, 2);
    fields->addWidget(acronym);
    root->addLayout(fields);

    auto *buttons = new QHBoxLayout;
    auto *add = new QPushButton(QStringLiteral("Add category"), &dialog);
    auto *cancel = new QPushButton(QStringLiteral("Cancel"), &dialog);
    auto *save = new QPushButton(QStringLiteral("Save categories"), &dialog);
    buttons->addWidget(add);
    buttons->addStretch(1);
    buttons->addWidget(cancel);
    buttons->addWidget(save);
    root->addLayout(buttons);

    const QRegularExpression idPattern(QStringLiteral("^\\d{2}$"));
    QObject::connect(add, &QPushButton::clicked, &dialog, [&]() {
        Pos::ProductCategory category{id->text().trimmed(), name->text().trimmed(),
            acronym->text().trimmed()};
        if (!idPattern.match(category.id).hasMatch()
            || category.name.isEmpty() || category.acronym.isEmpty()
            || category.name.contains(QLatin1Char(' '))
            || category.acronym.contains(QLatin1Char(' '))
            || category.name.contains(QLatin1Char('\t'))
            || category.acronym.contains(QLatin1Char('\t'))
            || category.name.contains(QLatin1Char('#'))
            || category.acronym.contains(QLatin1Char('#')))
        {
            QMessageBox::warning(&dialog, QStringLiteral("Invalid category"),
                QStringLiteral("Enter a 2-digit ID, a name and an acronym. "
                    "Text fields cannot contain spaces or #."));
            return;
        }
        for (const Pos::ProductCategory &existing : categories)
        {
            if (existing.id == category.id)
            {
                QMessageBox::warning(&dialog, QStringLiteral("Duplicate category"),
                    QStringLiteral("Category ID %1 is already configured.").arg(category.id));
                return;
            }
        }
        categories.append(category);
        appendCategory(category);
        id->clear();
        name->clear();
        acronym->clear();
        id->setFocus();
    });
    QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(save, &QPushButton::clicked, &dialog, [&]() {
        if (!Pos::saveProductCategories(categoryFile, categories, error))
        {
            QMessageBox::warning(&dialog, QStringLiteral("Cannot save categories"), error);
            return;
        }
        dialog.accept();
    });

    return dialog.exec() == QDialog::Accepted;
}
