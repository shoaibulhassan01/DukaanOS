#include "product_dialog.h"

#include "../../POS/product/product_service.h"

#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace
{
const QString kNavy = QStringLiteral("#111184");
const QString kInk = QStringLiteral("#20233B");
const QString kMuted = QStringLiteral("#777B91");
const QString kLine = QStringLiteral("#E7E9F3");

const QString kInputStyle = QStringLiteral(
    "QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox { background: #FAFAFD; color: %1;"
    " border: 1px solid %2; border-radius: 7px; padding: 0 9px; min-height: 32px;"
    " font-size: 11px; }"
    "QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus"
    " { background: white; border: 1px solid %3; }"
    "QLineEdit:disabled, QComboBox:disabled { color: %4; background: #F1F2F7; }")
    .arg(kInk, kLine, kNavy, kMuted);

enum class Mode { Add, Edit };

class ProductDialog : public QDialog
{
public:
    ProductDialog(QWidget *parent, const QString &productFile, Mode mode,
        const Pos::Product &product)
        : QDialog(parent), productFile_(productFile), mode_(mode), original_(product)
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
            ? QStringLiteral("All fields are saved to Product.txt. Barcode: 6 digits, not starting with 0. The barcode picture is created for you.")
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
        quantity_ = new QSpinBox(this);
        quantity_->setRange(0, 1000000);
        quantity_->setButtonSymbols(QAbstractSpinBox::NoButtons);
        image_ = new QLineEdit(this);
        image_->setPlaceholderText(QStringLiteral("Created from the barcode"));
        image_->setReadOnly(true);
        image_->setToolTip(QStringLiteral("The barcode picture is generated automatically when the product is added"));

        form->addRow(label(QStringLiteral("Product id")), id_);
        form->addRow(label(QStringLiteral("Barcode")), barcode_);
        form->addRow(label(QStringLiteral("Acronym")), acronym_);
        form->addRow(label(QStringLiteral("Name")), name_);
        form->addRow(label(QStringLiteral("Unit")), unit_);
        form->addRow(label(QStringLiteral("Unit price (PKR)")), price_);
        form->addRow(label(QStringLiteral("Tax per unit (PKR)")), tax_);
        form->addRow(label(QStringLiteral("Discount per unit (PKR)")), discount_);
        form->addRow(label(QStringLiteral("Available quantity")), quantity_);
        form->addRow(label(QStringLiteral("Barcode image")), image_);
        root->addLayout(form);

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
        }
        else
        {
            fill(product);
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

    QDoubleSpinBox *moneyBox()
    {
        auto *box = new QDoubleSpinBox(this);
        box->setRange(0, 10000000);
        box->setDecimals(2);
        box->setButtonSymbols(QAbstractSpinBox::NoButtons);
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
        price_->setValue(p.unitPrice);
        tax_->setValue(p.tax);
        discount_->setValue(p.discountPerUnit);
        quantity_->setValue(p.quantity);
        image_->setText(p.barcodeImage);
    }

    Pos::Product productFromForm() const
    {
        Pos::Product p;
        p.id = id_->text().trimmed();
        p.barcode = barcode_->text().trimmed();
        p.acronym = acronym_->text().trimmed();
        // Product.txt is space separated, so spaces in a name are stored as _.
        p.name = name_->text().simplified().replace(QLatin1Char(' '), QLatin1Char('_'));
        p.unit = unit_->currentText();
        p.unitPrice = price_->value();
        p.tax = tax_->value();
        p.discountPerUnit = discount_->value();
        p.quantity = quantity_->value();
        p.barcodeImage = image_->text().trimmed();
        return p;
    }

    void trySave()
    {
        QString error;
        const bool ok = mode_ == Mode::Add
            ? Pos::addProduct(productFile_, productFromForm(), error)
            : Pos::updateProduct(productFile_, original_.id, price_->value(),
                  discount_->value(), quantity_->value(), error);
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
    bool imageEdited_ = false;
    QLineEdit *id_ = nullptr;
    QLineEdit *barcode_ = nullptr;
    QLineEdit *acronym_ = nullptr;
    QLineEdit *name_ = nullptr;
    QComboBox *unit_ = nullptr;
    QDoubleSpinBox *price_ = nullptr;
    QDoubleSpinBox *tax_ = nullptr;
    QDoubleSpinBox *discount_ = nullptr;
    QSpinBox *quantity_ = nullptr;
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
