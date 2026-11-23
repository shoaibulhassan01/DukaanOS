#include "product_dialog.h"

#include "../../POS/category/category_service.h"
#include "../../POS/product/product_service.h"

#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QDoubleValidator>
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

namespace
{
const QString kNavy = QStringLiteral("#111184");
const QString kInk = QStringLiteral("#20233B");
const QString kMuted = QStringLiteral("#777B91");
const QString kLine = QStringLiteral("#E7E9F3");

const QString kInputStyle = QStringLiteral(
    "QLineEdit, QComboBox { background: #FAFAFD; color: %1;"
    " border: 1px solid %2; border-radius: 7px; padding: 0 9px; min-height: 32px;"
    " font-size: 11px; }"
    "QLineEdit:focus, QComboBox:focus"
    " { background: white; border: 1px solid %3; }"
    "QLineEdit:disabled, QComboBox:disabled { color: %4; background: #F1F2F7; }")
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
        setMinimumWidth(440);
        setStyleSheet(QStringLiteral("QDialog { background: white; }") + kInputStyle);

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(22, 20, 22, 18);
        root->setSpacing(12);

        auto *title = new QLabel(windowTitle(), this);
        title->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 16px; font-weight: 700;").arg(kInk));
        root->addWidget(title);

        auto *hint = new QLabel(mode == Mode::Add
            ? QStringLiteral("All fields are saved to Product.txt. Barcode: 6 digits; its first two digits are the Category Id (set up on the Categories page). The barcode picture is created for you.")
            : QStringLiteral("Only the price, discount and available stock can be changed."), this);
        hint->setWordWrap(true);
        hint->setStyleSheet(QStringLiteral("color: %1; font-size: 10px;").arg(kMuted));
        root->addWidget(hint);

        auto *form = new QFormLayout;
        form->setHorizontalSpacing(14);
        form->setVerticalSpacing(9);
        form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

        id_ = new QLineEdit(this);
        id_->setPlaceholderText(QStringLiteral("e.g. P0004"));
        barcode_ = new QLineEdit(this);
        barcode_->setPlaceholderText(QStringLiteral("e.g. 100004"));
        barcode_->setMaxLength(6);
        barcode_->setValidator(new QRegularExpressionValidator(
            QRegularExpression(QStringLiteral("\\d{0,6}")), barcode_));
        categoryInfo_ = new QLabel(this);
        categoryInfo_->setWordWrap(true);
        acronym_ = new QLineEdit(this);
        acronym_->setPlaceholderText(QStringLiteral("Up to 8 characters"));
        acronym_->setMaxLength(8);
        name_ = new QLineEdit(this);
        name_->setPlaceholderText(QStringLiteral("e.g. Green tea"));
        unit_ = new QComboBox(this);
        unit_->addItems({ QStringLiteral("count"), QStringLiteral("weight") });
        price_ = moneyBox();
        tax_ = moneyBox();
        discount_ = moneyBox();
        quantity_ = new QLineEdit(this);
        quantity_->setPlaceholderText(QStringLiteral("e.g. 10"));
        quantity_->setValidator(new QIntValidator(0, 1000000, quantity_));
        image_ = new QLineEdit(this);
        image_->setPlaceholderText(QStringLiteral("Created from the barcode"));
        image_->setReadOnly(true);
        image_->setToolTip(QStringLiteral("The barcode picture is generated automatically when the product is added"));

        form->addRow(label(QStringLiteral("Product id")), id_);
        form->addRow(label(QStringLiteral("Barcode")), barcode_);
        form->addRow(label(QStringLiteral("Category")), categoryInfo_);
        form->addRow(label(QStringLiteral("Acronym")), acronym_);
        form->addRow(label(QStringLiteral("Name")), name_);
        form->addRow(label(QStringLiteral("Unit")), unit_);
        form->addRow(label(QStringLiteral("Unit price (PKR)")), price_);
        form->addRow(label(QStringLiteral("Tax per unit (PKR)")), tax_);
        form->addRow(label(QStringLiteral("Discount per unit (PKR)")), discount_);
        form->addRow(label(QStringLiteral("Available quantity")), quantity_);
        form->addRow(label(QStringLiteral("Barcode image")), image_);

        auto *formContent = new QWidget(this);
        formContent->setLayout(form);
        auto *formScroll = new QScrollArea(this);
        formScroll->setWidgetResizable(true);
        formScroll->setFrameShape(QFrame::NoFrame);
        formScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        formScroll->setWidget(formContent);
        root->addWidget(formScroll, 1);

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
            const int maxHeight = qMax(360, screen->availableGeometry().height() * 9 / 10);
            setMaximumHeight(maxHeight);
            resize(qMax(width(), 500), qMin(700, maxHeight));
        }

        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
        connect(save, &QPushButton::clicked, this, [this]() { trySave(); });

        if (mode == Mode::Add)
        {
            // Suggest the image path from the barcode; it can still be edited.
            connect(barcode_, &QLineEdit::textChanged, this, [this](const QString &text) {
                if (!imageEdited_)
                    image_->setText(text.trimmed().isEmpty()
                        ? QString() : QStringLiteral("Data/barcodes/%1.png").arg(text.trimmed()));
            });
            connect(image_, &QLineEdit::textEdited, this, [this]() { imageEdited_ = true; });
            // Req 1: the first two digits of the barcode pick the category.
            connect(barcode_, &QLineEdit::textChanged, this,
                [this](const QString &text) { showCategory(text); });
            showCategory(QString());
        }
        else
        {
            fill(product);
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
                text = error.startsWith(QStringLiteral("No category"))
                    ? QStringLiteral("Category %1 not found. Add it on the Categories page first.")
                          .arg(barcode.left(2))
                    : error;
                color = QStringLiteral("#B42318");
            }
        }
        categoryInfo_->setText(text);
        categoryInfo_->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; font-weight: 600;").arg(color));
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
        unit_->setCurrentText(p.unit);
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
        p.acronym = acronym_->text().trimmed();
        // Product.txt is space separated, so spaces in a name are stored as _.
        p.name = name_->text().simplified().replace(QLatin1Char(' '), QLatin1Char('_'));
        p.unit = unit_->currentText();
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
    bool imageEdited_ = false;
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
