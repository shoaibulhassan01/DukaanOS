#include "new_order_page.h"

#include "../../BarCode/camera/camera_scanner.h"
#include "../../POS/product/product.h"
#include "../../POS/product/product_repository.h"
#include "../../POS/product/product_service.h"
#include "../../POS/checkout/checkout_service.h"
#include "../../POS/common/data_paths.h"
#include "../../POS/payment/payment_method.h"
#include "product_details_view.h"
#include "wheel_guard.h"

#include <QCoreApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSignalBlocker>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QImage>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMediaDevices>
#include <QPixmap>
#include <QPushButton>
#include <QTableWidgetItem>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>
#include <utility>

namespace
{
const QString kNavy = QStringLiteral("#111184");
const QString kInk = QStringLiteral("#20233B");
const QString kMuted = QStringLiteral("#777B91");
const QString kLine = QStringLiteral("#E7E9F3");

QFrame *createCard(QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("orderCard"));
    card->setStyleSheet(QStringLiteral(
        "QFrame#orderCard { background: white; border: 1px solid %1; border-radius: 14px; }")
        .arg(kLine));
    return card;
}

QLabel *createSectionTitle(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 14px; font-weight: 700; background: transparent;").arg(kInk));
    return label;
}

QLabel *createFieldLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setStyleSheet(QStringLiteral(
        "color: #55596F; font-size: 11px; font-weight: 600; background: transparent;"));
    return label;
}

QString fieldStyle()
{
    return QStringLiteral(
        "QLineEdit, QSpinBox, QComboBox { background: #FAFAFD; color: %1;"
        " border: 1px solid %2; border-radius: 9px; padding: 0 11px; font-size: 12px; }"
        "QLineEdit:focus, QSpinBox:focus, QComboBox:focus { background: white;"
        " border: 1px solid %3; }")
        .arg(kInk, kLine, kNavy);
}

class NewOrderPage : public QWidget
{
    struct CartLine
    {
        Pos::Product product;
        int quantity = 0;
    };

public:
    explicit NewOrderPage(QWidget *parent, const OrderPreview *initialOrder,
        std::function<void(const OrderPreview &)> onGenerateBill,
        std::function<void(const OrderPreview &)> onHoldOrder)
        : QWidget(parent), onGenerateBill_(std::move(onGenerateBill)),
          onHoldOrder_(std::move(onHoldOrder))
    {
        scanner_ = new CameraBarcodeScanner(this);
        scanner_->setHandlers(
            [this](const QImage &image) {
                if (!previewLabel_->size().isEmpty())
                {
                    previewLabel_->setPixmap(QPixmap::fromImage(image).scaled(
                        previewLabel_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
                }
            },
            [this](const QString &barcode) {
                stopCamera();
                barcodeEdit_->setText(barcode);
                submitBarcode(barcode);
            },
            [this](const QString &error) {
                stopCamera();
                setStatus(error, true);
            });

        // The QToolTip rule stops tooltips inheriting the transparent
        // background (which made them show as a black bar).
        setStyleSheet(QStringLiteral("background: transparent;"
            "QToolTip { background: #20233B; color: white; border: none; padding: 5px 8px; font-size: 11px; }"));
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 18);
        root->setSpacing(16);

        root->addWidget(createCustomerCard());

        auto *body = new QHBoxLayout;
        body->setSpacing(16);
        body->addWidget(createItemsCard(), 7);
        body->addWidget(createSummaryCard(), 4);
        root->addLayout(body);

        auto *notice = new QLabel(
            QStringLiteral("When the bill is generated it is saved to Order.txt, OrderDetail.txt, Bill.txt and BillDetail.txt, stock is reduced, and the bill opens."),
            this);
        notice->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; background: transparent; padding-left: 2px;")
            .arg(kMuted));
        root->addWidget(notice);
        if (initialOrder)
            loadOrder(*initialOrder);
        updatePaymentFields();
    }

    ~NewOrderPage() override
    {
        scanner_->stop();
    }

private:
    QWidget *createCustomerCard()
    {
        auto *card = createCard(this);
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(20, 17, 20, 18);
        layout->setSpacing(12);

        auto *heading = new QHBoxLayout;
        heading->addWidget(createSectionTitle(QStringLiteral("Customer details"), card));
        heading->addStretch(1);
        auto *optional = new QLabel(QStringLiteral("Optional"), card);
        optional->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; background: transparent;").arg(kMuted));
        heading->addWidget(optional);
        layout->addLayout(heading);

        auto *fields = new QGridLayout;
        fields->setHorizontalSpacing(14);
        fields->setVerticalSpacing(7);
        fields->addWidget(createFieldLabel(QStringLiteral("Customer name"), card), 0, 0);
        fields->addWidget(createFieldLabel(QStringLiteral("Phone number"), card), 0, 1);

        customerNameEdit_ = new QLineEdit(card);
        customerNameEdit_->setPlaceholderText(QStringLiteral("Enter customer name"));
        customerNameEdit_->setFixedHeight(42);
        customerNameEdit_->setStyleSheet(fieldStyle());
        fields->addWidget(customerNameEdit_, 1, 0);

        customerPhoneEdit_ = new QLineEdit(card);
        customerPhoneEdit_->setPlaceholderText(QStringLiteral("03XX XXXXXXX"));
        customerPhoneEdit_->setFixedHeight(42);
        customerPhoneEdit_->setMaxLength(16);
        customerPhoneEdit_->setStyleSheet(fieldStyle());
        fields->addWidget(customerPhoneEdit_, 1, 1);
        fields->setColumnStretch(0, 1);
        fields->setColumnStretch(1, 1);
        layout->addLayout(fields);
        return card;
    }

    QWidget *createItemsCard()
    {
        auto *card = createCard(this);
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(18, 17, 18, 16);
        layout->setSpacing(12);

        auto *heading = new QHBoxLayout;
        auto *title = createSectionTitle(QStringLiteral("Add products"), card);
        heading->addWidget(title);
        heading->addStretch(1);

        cameraButton_ = new QPushButton(QStringLiteral("  Scan with camera"), card);
        cameraButton_->setCursor(Qt::PointingHandCursor);
        cameraButton_->setFixedHeight(36);
        cameraButton_->setStyleSheet(QStringLiteral(
            "QPushButton { background: #F0F0FF; color: %1; border: 1px solid #E1E2FA;"
            " border-radius: 8px; padding: 0 10px; font-size: 10px; font-weight: 600; }"
            "QPushButton:hover { background: #E8E9FF; }").arg(kNavy));
        heading->addWidget(cameraButton_);
        layout->addLayout(heading);

        auto *hint = new QLabel(
            QStringLiteral("Scan a barcode to add it immediately, or enter it below and press Enter."),
            card);
        hint->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; background: transparent;").arg(kMuted));
        layout->addWidget(hint);

        previewLabel_ = new QLabel(card);
        previewLabel_->setAlignment(Qt::AlignCenter);
        previewLabel_->setFixedHeight(145);
        previewLabel_->setStyleSheet(QStringLiteral(
            "background: #15182A; color: white; border-radius: 10px;"));
        previewLabel_->setText(QStringLiteral("Camera preview"));
        previewLabel_->hide();
        layout->addWidget(previewLabel_);

        auto *entry = new QHBoxLayout;
        entry->setSpacing(9);
        barcodeEdit_ = new QLineEdit(card);
        barcodeEdit_->setPlaceholderText(QStringLiteral("Enter product barcode"));
        barcodeEdit_->setFixedHeight(42);
        barcodeEdit_->setStyleSheet(fieldStyle());
        entry->addWidget(barcodeEdit_, 1);
        barcodeLookupTimer_ = new QTimer(this);
        barcodeLookupTimer_->setSingleShot(true);
        barcodeLookupTimer_->setInterval(250);
        connect(barcodeLookupTimer_, &QTimer::timeout, this, [this]() {
            lookupBarcode(barcodeEdit_->text());
        });

        // Quantity stepper:  [ - ]  qty  [ + ]  (type a number or click).
        auto *stepper = new QFrame(card);
        stepper->setObjectName(QStringLiteral("qtyStepper"));
        stepper->setFixedHeight(42);
        stepper->setStyleSheet(QStringLiteral(
            "QFrame#qtyStepper { background: #FAFAFD; border: 1px solid %1; border-radius: 9px; }"
            "QPushButton { background: transparent; color: %2; border: none; font-size: 17px;"
            " font-weight: 700; min-width: 30px; }"
            "QPushButton:hover { background: #F0F0FF; border-radius: 7px; }"
            "QPushButton:disabled { color: #C9CAD6; }"
            "QLineEdit { background: transparent; color: %3; border: none; font-size: 15px;"
            " font-weight: 700; }").arg(kLine, kNavy, kInk));
        auto *stepperLayout = new QHBoxLayout(stepper);
        stepperLayout->setContentsMargins(4, 4, 4, 4);
        stepperLayout->setSpacing(0);
        auto *minus = new QPushButton(QString(QChar(0x2212)), stepper);
        minus->setCursor(Qt::PointingHandCursor);
        minus->setFocusPolicy(Qt::NoFocus);
        quantity_ = new QLineEdit(stepper);
        quantity_->setValidator(new QIntValidator(1, 999, quantity_));
        quantity_->setText(QStringLiteral("1"));
        quantity_->setAlignment(Qt::AlignCenter);
        quantity_->setFixedWidth(56);
        auto *plus = new QPushButton(QStringLiteral("+"), stepper);
        plus->setCursor(Qt::PointingHandCursor);
        plus->setFocusPolicy(Qt::NoFocus);
        stepperLayout->addWidget(minus);
        stepperLayout->addWidget(quantity_);
        stepperLayout->addWidget(plus);
        connect(minus, &QPushButton::clicked, this, [this]() { stepQuantity(-1); });
        connect(plus, &QPushButton::clicked, this, [this]() { stepQuantity(1); });
        auto updateStepButtons = [this, minus, plus]() {
            bool valid = false;
            const int quantity = quantity_->text().toInt(&valid);
            minus->setEnabled(valid && quantity > 1);
            plus->setEnabled(!valid || quantity < 999);
        };
        connect(quantity_, &QLineEdit::textChanged, this, updateStepButtons);
        updateStepButtons();
        entry->addWidget(stepper);

        descriptionButton_ = new QPushButton(QStringLiteral("Description"), card);
        descriptionButton_->setCursor(Qt::PointingHandCursor);
        descriptionButton_->setFixedSize(104, 42);
        descriptionButton_->setStyleSheet(QStringLiteral(
            "QPushButton { background: #F0F0FF; color: %1; border: 1px solid #E1E2FA;"
            " border-radius: 9px; font-size: 11px; font-weight: 700; }"
            "QPushButton:hover { background: #E8E9FF; }").arg(kNavy));
        entry->addWidget(descriptionButton_);
        layout->addLayout(entry);

        auto *stockHint = new QLabel(
            QStringLiteral("Matched unit price is read from Product.txt. Required quantity must be less than stock."),
            card);
        stockHint->setWordWrap(true);
        stockHint->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; background: transparent;").arg(kMuted));
        layout->addWidget(stockHint);

        matchLabel_ = new QLabel(card);
        matchLabel_->setWordWrap(true);
        matchLabel_->setStyleSheet(QStringLiteral(
            "background: #F0FBF6; color: #18764A; border: 1px solid #D6F1E3;"
            " border-radius: 8px; padding: 9px; font-size: 11px;"));
        matchLabel_->hide();
        layout->addWidget(matchLabel_);

        statusLabel_ = new QLabel(card);
        statusLabel_->setWordWrap(true);
        statusLabel_->setStyleSheet(QStringLiteral(
            "color: #B42318; font-size: 10px; background: transparent;"));
        statusLabel_->hide();
        layout->addWidget(statusLabel_);

        auto *table = new QTableWidget(card);
        cartTable_ = table;
        table->setColumnCount(5);
        table->setHorizontalHeaderLabels({
            QStringLiteral("PRODUCT"), QStringLiteral("BARCODE"),
            QStringLiteral("UNIT PRICE"), QStringLiteral("QTY"),
            QStringLiteral("TOTAL")
        });
        table->setRowCount(1);
        table->setSpan(0, 0, 1, 5);
        auto *empty = new QLabel(
            QStringLiteral("<b>Your order is empty</b><br><span style='color:#777B91;'>"
                           "Scan a barcode to start adding products.</span>"), table);
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 11px; background: transparent;").arg(kMuted));
        table->setCellWidget(0, 0, empty);
        table->setRowHeight(0, 118);
        table->verticalHeader()->hide();
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->horizontalHeader()->setFixedHeight(38);
        table->setShowGrid(false);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionMode(QAbstractItemView::NoSelection);
        table->setFocusPolicy(Qt::NoFocus);
        table->setStyleSheet(QStringLiteral(
            "QTableWidget { background: white; border: none; color: %1; }"
            "QHeaderView::section { background: #F8F8FC; color: %2; border: none;"
            " border-bottom: 1px solid %3; padding: 0 8px; font-size: 9px; font-weight: 700; }")
            .arg(kInk, kMuted, kLine));
        layout->addWidget(table, 1);

        connect(cameraButton_, &QPushButton::clicked, this, [this]() { toggleCamera(); });
        connect(barcodeEdit_, &QLineEdit::returnPressed, this,
            [this]() { submitBarcode(barcodeEdit_->text()); });
        connect(barcodeEdit_, &QLineEdit::textChanged, this, [this](const QString &text) {
            hasMatchedProduct_ = false;
            matchLabel_->hide();
            if (text.trimmed().isEmpty())
                barcodeLookupTimer_->stop();
            else
                barcodeLookupTimer_->start();
        });
        connect(descriptionButton_, &QPushButton::clicked, this, [this]() { showDescription(); });
        return card;
    }

    QWidget *createSummaryCard()
    {
        auto *card = createCard(this);
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(18, 18, 18, 18);
        layout->setSpacing(0);

        layout->addWidget(createSectionTitle(QStringLiteral("Order summary"), card));
        layout->addSpacing(5);
        auto *subtitle = new QLabel(QStringLiteral("Review totals before checkout"), card);
        subtitle->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; background: transparent;").arg(kMuted));
        layout->addWidget(subtitle);
        layout->addSpacing(20);

        itemsValue_ = addSummaryRow(layout, card, QStringLiteral("Items"), QStringLiteral("0"));
        subtotalValue_ = addSummaryRow(
            layout, card, QStringLiteral("Subtotal"), QStringLiteral("PKR 0"));
        taxValue_ = addSummaryRow(layout, card, QStringLiteral("Tax"), QStringLiteral("PKR 0"));
        discountValue_ = addSummaryRow(
            layout, card, QStringLiteral("Discount"), QStringLiteral("PKR 0"));

        auto *divider = new QFrame(card);
        divider->setFrameShape(QFrame::HLine);
        divider->setStyleSheet(QStringLiteral("color: %1;").arg(kLine));
        layout->addSpacing(3);
        layout->addWidget(divider);
        layout->addSpacing(13);

        auto *total = new QHBoxLayout;
        auto *totalLabel = new QLabel(QStringLiteral("Total"), card);
        totalLabel->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 13px; font-weight: 700; background: transparent;").arg(kInk));
        totalValue_ = new QLabel(QStringLiteral("PKR 0"), card);
        totalValue_->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 18px; font-weight: 700; background: transparent;").arg(kNavy));
        total->addWidget(totalLabel);
        total->addStretch(1);
        total->addWidget(totalValue_);
        layout->addLayout(total);
        layout->addSpacing(20);

        layout->addWidget(createFieldLabel(QStringLiteral("Payment method"), card));
        layout->addSpacing(7);
        paymentMethod_ = new QComboBox(card);
        paymentMethod_->addItems({QStringLiteral("Cash"), QStringLiteral("Card"),
            QStringLiteral("Wallet")});
        paymentMethod_->setFixedHeight(42);
        paymentMethod_->setStyleSheet(fieldStyle());
        layout->addWidget(paymentMethod_);
        layout->addSpacing(10);

        // Card / Wallet: bank or wallet name. Cash: amount received and change.
        merchantLabel_ = createFieldLabel(QStringLiteral("Bank / wallet name"), card);
        layout->addWidget(merchantLabel_);
        layout->addSpacing(7);
        merchantEdit_ = new QLineEdit(card);
        merchantEdit_->setPlaceholderText(QStringLiteral("e.g. HBL, ABL, JazzCash, Easypaisa"));
        merchantEdit_->setFixedHeight(42);
        merchantEdit_->setStyleSheet(fieldStyle());
        layout->addWidget(merchantEdit_);

        cashLabel_ = createFieldLabel(QStringLiteral("Cash received (PKR)"), card);
        layout->addWidget(cashLabel_);
        layout->addSpacing(7);
        cashReceived_ = new QDoubleSpinBox(card);
        cashReceived_->setRange(0, 100000000);
        cashReceived_->setDecimals(2);
        cashReceived_->setButtonSymbols(QAbstractSpinBox::NoButtons);
        UiPages::ignoreMouseWheel(cashReceived_);
        cashReceived_->setFixedHeight(42);
        cashReceived_->setStyleSheet(fieldStyle());
        layout->addWidget(cashReceived_);
        layout->addSpacing(6);
        changeLabel_ = new QLabel(card);
        changeLabel_->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; background: transparent;").arg(kMuted));
        layout->addWidget(changeLabel_);
        layout->addSpacing(14);
        connect(paymentMethod_, &QComboBox::currentTextChanged, this, [this]() { updatePaymentFields(); });
        connect(cashReceived_, &QDoubleSpinBox::valueChanged, this, [this]() { updatePaymentFields(); });
        connect(cashReceived_, &QAbstractSpinBox::editingFinished, this, [this]() { cashEdited_ = true; });

        auto *hold = new QPushButton(QStringLiteral("Hold order"), card);
        holdButton_ = hold;
        hold->setCursor(Qt::PointingHandCursor);
        hold->setFixedHeight(42);
        hold->setStyleSheet(QStringLiteral(
            "QPushButton { background: white; color: %1; border: 1px solid %2;"
            " border-radius: 9px; font-size: 11px; font-weight: 600; }"
            "QPushButton:hover { background: #F8F8FC; }").arg(kNavy, kLine));
        layout->addWidget(hold);
        layout->addSpacing(9);

        auto *checkout = new QPushButton(QStringLiteral("Generate bill"), card);
        checkoutButton_ = checkout;
        checkout->setCursor(Qt::PointingHandCursor);
        checkout->setFixedHeight(46);
        checkout->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; color: white; border: none; border-radius: 9px;"
            " font-size: 12px; font-weight: 700; }"
            "QPushButton:hover { background: #0D0D6B; }").arg(kNavy));
        layout->addWidget(checkout);
        layout->addSpacing(8);
        checkoutMessage_ = new QLabel(card);
        checkoutMessage_->setWordWrap(true);
        checkoutMessage_->setStyleSheet(QStringLiteral(
            "color: #B42318; background: #FEF3F2; border: 1px solid #FECDCA;"
            " border-radius: 8px; padding: 8px 10px; font-size: 11px;"));
        checkoutMessage_->hide();
        layout->addWidget(checkoutMessage_);
        layout->addStretch(1);
        connect(holdButton_, &QPushButton::clicked, this, [this]() { holdOrder(); });
        connect(checkoutButton_, &QPushButton::clicked, this, [this]() { generateBill(); });
        return card;
    }

    QLabel *addSummaryRow(QVBoxLayout *layout, QWidget *parent,
        const QString &labelText, const QString &valueText)
    {
        auto *row = new QHBoxLayout;
        auto *label = new QLabel(labelText, parent);
        label->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 11px; background: transparent;").arg(kMuted));
        auto *value = new QLabel(valueText, parent);
        value->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 11px; font-weight: 600; background: transparent;").arg(kInk));
        row->addWidget(label);
        row->addStretch(1);
        row->addWidget(value);
        layout->addLayout(row);
        layout->addSpacing(12);
        return value;
    }

    QString productFilePath() const
    {
        return Pos::DataPaths::products();
    }

    void toggleCamera()
    {
        if (scanner_->isRunning())
        {
            stopCamera();
            setStatus(QStringLiteral("Camera scanning stopped."));
            return;
        }

        const QCameraDevice camera = QMediaDevices::defaultVideoInput();
        if (camera.isNull())
        {
            setStatus(QStringLiteral("No camera is available on this device."), true);
            return;
        }

        previewLabel_->clear();
        previewLabel_->setText(QStringLiteral("Starting camera..."));
        previewLabel_->show();
        cameraButton_->setText(QStringLiteral("  Stop camera"));

        QString error;
        if (!scanner_->start(camera, error))
        {
            stopCamera();
            setStatus(error, true);
            return;
        }
        setStatus(QStringLiteral("Point the camera at a product barcode."));
    }

    void stopCamera()
    {
        scanner_->stop();
        previewLabel_->clear();
        previewLabel_->setText(QStringLiteral("Camera preview"));
        previewLabel_->hide();
        cameraButton_->setText(QStringLiteral("  Scan with camera"));
    }

    void setStatus(const QString &message, bool isError = false)
    {
        statusLabel_->setText(message);
        statusLabel_->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; background: transparent;")
            .arg(isError ? QStringLiteral("#B42318") : QStringLiteral("#52607A")));
        statusLabel_->show();
    }

    void lookupBarcode(QString barcode)
    {
        barcode = barcode.trimmed();
        if (barcode.isEmpty())
        {
            hasMatchedProduct_ = false;
            matchLabel_->hide();
            setStatus(QStringLiteral("Enter or scan a product barcode first."), true);
            return;
        }

        QVector<Pos::Product> products;
        QString error;
        const QString filePath = productFilePath();
        if (!Pos::readProducts(filePath, products, error))
        {
            hasMatchedProduct_ = false;
            matchLabel_->hide();
            setStatus(error, true);
            return;
        }

        if (!Pos::findProductByBarcode(products, barcode, currentProduct_))
        {
            hasMatchedProduct_ = false;
            matchLabel_->hide();
            for (const Pos::Product &product : products)
            {
                if (product.barcode.startsWith(barcode))
                {
                    setStatus(QStringLiteral("Keep entering the product barcode."));
                    return;
                }
            }
            setStatus(QStringLiteral("No product in Product.txt matches barcode %1.")
                .arg(barcode), true);
            return;
        }

        hasMatchedProduct_ = true;
        QString productName = currentProduct_.name;
        productName.replace(QLatin1Char('_'), QLatin1Char(' '));
        matchLabel_->setText(QStringLiteral(
            "<b>%1</b><br>Unit price: PKR %2  |  Available stock: %3")
            .arg(productName.toHtmlEscaped())
            .arg(QString::number(currentProduct_.unitPrice, 'f', 2))
            .arg(currentProduct_.quantity));
        matchLabel_->show();
        setStatus(QStringLiteral("Product matched from Product.txt."));
    }

    void submitBarcode(QString barcode)
    {
        barcodeLookupTimer_->stop();
        lookupBarcode(barcode);
        if (hasMatchedProduct_)
            addProductToCart();
    }

    void stepQuantity(int change)
    {
        bool valid = false;
        int quantity = quantity_->text().toInt(&valid);
        if (!valid)
            quantity = 1;
        quantity_->setText(QString::number(qBound(1, quantity + change, 999)));
    }

    void addProductToCart()
    {
        if (!hasMatchedProduct_)
        {
            lookupBarcode(barcodeEdit_->text());
            if (!hasMatchedProduct_)
                return;
        }

        bool validQuantity = false;
        const int requestedQuantity = quantity_->text().toInt(&validQuantity);
        if (!validQuantity || requestedQuantity < 1)
        {
            setStatus(QStringLiteral("Enter a quantity from 1 to 999."), true);
            return;
        }

        int existingQuantity = 0;
        int existingRow = -1;
        for (int i = 0; i < cartLines_.size(); ++i)
        {
            if (cartLines_.at(i).product.barcode == currentProduct_.barcode)
            {
                existingQuantity = cartLines_.at(i).quantity;
                existingRow = i;
                break;
            }
        }

        // Requirement 5: the required quantity must be less than the stock.
        if (!Pos::checkStock(currentProduct_, existingQuantity + requestedQuantity))
        {
            setStatus(QStringLiteral(
                "Required quantity must be less than the available stock (%1).")
                .arg(currentProduct_.quantity), true);
            return;
        }

        if (existingRow < 0)
            cartLines_.append(CartLine{currentProduct_, requestedQuantity});
        else
            cartLines_[existingRow].quantity += requestedQuantity;

        refreshCart();
        setStatus(QStringLiteral("Product added to the order preview."));
        quantity_->setText(QStringLiteral("1"));
        barcodeEdit_->clear();
        barcodeEdit_->setFocus();
    }

    void adjustCartQuantity(int row, int change)
    {
        if (row < 0 || row >= cartLines_.size())
            return;

        CartLine &line = cartLines_[row];
        if (change > 0 && !Pos::checkStock(line.product, line.quantity + change))
        {
            setStatus(QStringLiteral(
                "Required quantity must be less than the available stock (%1).")
                .arg(line.product.quantity), true);
            return;
        }

        line.quantity = qMax(1, line.quantity + change);
        refreshCart();
    }

    void refreshCart()
    {
        cartTable_->clearSpans();
        cartTable_->setRowCount(0);   // also removes the "order is empty" message
        cartTable_->setRowCount(cartLines_.size());
        int itemCount = 0;
        double subtotal = 0;
        double tax = 0;
        double discount = 0;

        for (int row = 0; row < cartLines_.size(); ++row)
        {
            const CartLine &line = cartLines_.at(row);
            QString productName = line.product.name;
            productName.replace(QLatin1Char('_'), QLatin1Char(' '));
            const double lineSubtotal = line.product.unitPrice * line.quantity;
            itemCount += line.quantity;
            subtotal += lineSubtotal;
            tax += line.product.tax * line.quantity;
            discount += line.product.discountPerUnit * line.quantity;

            auto setCell = [this, row](int column, const QString &value) {
                auto *cell = new QTableWidgetItem(value);
                cell->setFlags(cell->flags() & ~Qt::ItemIsEditable);
                cartTable_->setItem(row, column, cell);
            };
            setCell(0, productName);
            setCell(1, line.product.barcode);
            setCell(2, QStringLiteral("PKR %1").arg(line.product.unitPrice, 0, 'f', 2));

            auto *quantityControl = new QWidget(cartTable_);
            auto *quantityLayout = new QHBoxLayout(quantityControl);
            quantityLayout->setContentsMargins(2, 1, 2, 1);
            quantityLayout->setSpacing(2);
            auto makeQuantityButton = [quantityControl](const QString &text) {
                auto *button = new QPushButton(text, quantityControl);
                button->setFixedSize(24, 24);
                button->setCursor(Qt::PointingHandCursor);
                button->setStyleSheet(QStringLiteral(
                    "QPushButton { background: #F0F0FF; color: #111184; border: 1px solid #E1E2FA;"
                    " border-radius: 6px; font-size: 13px; font-weight: 700; padding: 0; }"
                    "QPushButton:hover { background: #E1E2FA; }"));
                return button;
            };
            auto *decrease = makeQuantityButton(QString(QChar(0x2212)));
            auto *quantity = new QLabel(QString::number(line.quantity), quantityControl);
            quantity->setAlignment(Qt::AlignCenter);
            quantity->setMinimumWidth(24);
            quantity->setStyleSheet(QStringLiteral(
                "color: %1; font-size: 11px; font-weight: 700; background: transparent;")
                .arg(kInk));
            auto *increase = makeQuantityButton(QStringLiteral("+"));
            quantityLayout->addStretch(1);
            quantityLayout->addWidget(decrease);
            quantityLayout->addWidget(quantity);
            quantityLayout->addWidget(increase);
            quantityLayout->addStretch(1);
            connect(decrease, &QPushButton::clicked, this,
                [this, row]() { adjustCartQuantity(row, -1); });
            connect(increase, &QPushButton::clicked, this,
                [this, row]() { adjustCartQuantity(row, 1); });
            decrease->setEnabled(line.quantity > 1);
            increase->setEnabled(line.quantity + 1 < line.product.quantity);
            cartTable_->setCellWidget(row, 3, quantityControl);

            setCell(4, QStringLiteral("PKR %1").arg(lineSubtotal, 0, 'f', 2));
            cartTable_->setRowHeight(row, 42);
        }

        itemsValue_->setText(QString::number(itemCount));
        subtotalValue_->setText(QStringLiteral("PKR %1").arg(subtotal, 0, 'f', 2));
        taxValue_->setText(QStringLiteral("PKR %1").arg(tax, 0, 'f', 2));
        discountValue_->setText(QStringLiteral("PKR %1").arg(discount, 0, 'f', 2));
        totalValue_->setText(QStringLiteral("PKR %1").arg(subtotal + tax - discount, 0, 'f', 2));
        currentTotal_ = subtotal + tax - discount;
        // Until the cashier types an amount, cash received equals the total.
        if (cashReceived_ && !cashEdited_)
        {
            const QSignalBlocker block(cashReceived_);
            cashReceived_->setValue(currentTotal_);
        }
        updatePaymentFields();
    }

    void updatePaymentFields()
    {
        if (!paymentMethod_ || !cashReceived_)
            return;
        const bool cash = paymentMethod_->currentText() == QStringLiteral("Cash");
        merchantLabel_->setVisible(!cash);
        merchantEdit_->setVisible(!cash);
        cashLabel_->setVisible(cash);
        cashReceived_->setVisible(cash);
        changeLabel_->setVisible(cash);
        const double change = cashReceived_->value() - currentTotal_;
        changeLabel_->setText(cashReceived_->value() <= 0
            ? QStringLiteral("Enter the cash given by the customer.")
            : change < -0.005
                ? QStringLiteral("PKR %1 still due").arg(-change, 0, 'f', 2)
                : QStringLiteral("Change to return: PKR %1").arg(change, 0, 'f', 2));
    }

    void showDescription()
    {
        if (!hasMatchedProduct_)
        {
            lookupBarcode(barcodeEdit_->text());
            if (!hasMatchedProduct_)
                return;
        }
        UiPages::showProductDescription(currentProduct_, this);
    }

    OrderPreview createOrderPreview() const
    {
        OrderPreview order;
        order.orderId = orderId_;
        order.customerName = customerNameEdit_->text().trimmed();
        order.phone = customerPhoneEdit_->text().trimmed();
        order.paymentMethod = paymentMethod_->currentText();
        for (const CartLine &line : cartLines_)
        {
            order.lines.append(OrderPreviewLine{line.product, line.quantity});
            order.itemCount += line.quantity;
            order.subtotal += line.product.unitPrice * line.quantity;
            order.tax += line.product.tax * line.quantity;
            order.discount += line.product.discountPerUnit * line.quantity;
        }
        order.total = order.subtotal + order.tax - order.discount;
        return order;
    }

    void loadOrder(const OrderPreview &order)
    {
        orderId_ = order.orderId;
        customerNameEdit_->setText(order.customerName);
        customerPhoneEdit_->setText(order.phone);
        const int paymentIndex = paymentMethod_->findText(order.paymentMethod);
        if (paymentIndex >= 0)
            paymentMethod_->setCurrentIndex(paymentIndex);
        cartLines_.clear();
        for (const OrderPreviewLine &line : order.lines)
            cartLines_.append(CartLine{line.product, line.quantity});
        refreshCart();
    }

    void holdOrder()
    {
        OrderPreview order = createOrderPreview();
        if (order.lines.isEmpty())
        {
            setStatus(QStringLiteral("Add at least one product before holding an order."), true);
            return;
        }
        if (onHoldOrder_)
            onHoldOrder_(order);
    }

    void generateBill()
    {
        OrderPreview order = createOrderPreview();
        if (order.lines.isEmpty())
        {
            checkoutMessage_->setText(QStringLiteral("Add at least one product before generating a bill."));
            checkoutMessage_->show();
            return;
        }
        // Save the order, bill header/details and stock changes in checkout.
        Pos::CheckoutRequest request;
        request.customerName = order.customerName;
        request.customerContact = order.phone;
        request.existingOrderId = orderId_;
        request.paymentMode = paymentMethod_->currentText();
        const bool cash = request.paymentMode == QStringLiteral("Cash");
        request.merchant = cash ? QString() : merchantEdit_->text().trimmed();
        request.amountPaid = cash ? cashReceived_->value() : order.total;
        for (const CartLine &line : cartLines_)
            request.items.append(Pos::CartItem{line.product, line.quantity});

        Pos::CheckoutResult result;
        QString error;
        if (!Pos::generateBill(request, result, error))
        {
            checkoutMessage_->setText(QStringLiteral("Bill not saved: %1").arg(error));
            checkoutMessage_->show();
            return;
        }
        checkoutMessage_->hide();

        order.id = result.bill.id;
        order.orderId = result.order.id;
        order.status = result.order.status;
        order.customerName = result.order.customerName;
        order.createdAt = QStringLiteral("%1  %2").arg(result.order.date, result.order.time);
        order.paymentMethod = cash
            ? QStringLiteral("Cash (received PKR %1, change PKR %2)")
                  .arg(request.amountPaid, 0, 'f', 2)
                  .arg(request.amountPaid - result.totals.payable, 0, 'f', 2)
            : QStringLiteral("%1 (%2)").arg(paymentMethod_->currentText(), request.merchant);
        order.subtotal = result.totals.withoutTax;
        order.tax = result.totals.tax;
        order.discount = result.totals.discount;
        order.total = result.totals.payable;
        if (onGenerateBill_)
            onGenerateBill_(order);
    }

    CameraBarcodeScanner *scanner_ = nullptr;
    QPushButton *cameraButton_ = nullptr;
    QPushButton *descriptionButton_ = nullptr;
    QLabel *merchantLabel_ = nullptr;
    QLineEdit *merchantEdit_ = nullptr;
    QLabel *cashLabel_ = nullptr;
    QDoubleSpinBox *cashReceived_ = nullptr;
    QLabel *changeLabel_ = nullptr;
    double currentTotal_ = 0;
    bool cashEdited_ = false;
    QLabel *checkoutMessage_ = nullptr;
    QPushButton *holdButton_ = nullptr;
    QPushButton *checkoutButton_ = nullptr;
    QLineEdit *barcodeEdit_ = nullptr;
    QTimer *barcodeLookupTimer_ = nullptr;
    QLineEdit *customerNameEdit_ = nullptr;
    QLineEdit *customerPhoneEdit_ = nullptr;
    QLineEdit *quantity_ = nullptr;
    QComboBox *paymentMethod_ = nullptr;
    QLabel *previewLabel_ = nullptr;
    QLabel *matchLabel_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QTableWidget *cartTable_ = nullptr;
    QLabel *itemsValue_ = nullptr;
    QLabel *subtotalValue_ = nullptr;
    QLabel *taxValue_ = nullptr;
    QLabel *discountValue_ = nullptr;
    QLabel *totalValue_ = nullptr;
    Pos::Product currentProduct_;
    QString orderId_;
    QVector<CartLine> cartLines_;
    bool hasMatchedProduct_ = false;
    std::function<void(const OrderPreview &)> onGenerateBill_;
    std::function<void(const OrderPreview &)> onHoldOrder_;
};
}

QWidget *UiPages::createNewOrderPage(QWidget *parent, const OrderPreview *initialOrder,
    std::function<void(const OrderPreview &)> onGenerateBill,
    std::function<void(const OrderPreview &)> onHoldOrder)
{
    return new NewOrderPage(parent, initialOrder,
        std::move(onGenerateBill), std::move(onHoldOrder));
}
