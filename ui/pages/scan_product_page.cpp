#include "scan_product_page.h"

#include "product_details_view.h"
#include "../../BarCode/camera/camera_scanner.h"
#include "../../POS/common/data_paths.h"
#include "../../POS/product/product_repository.h"
#include "../../POS/product/product_service.h"

#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMediaDevices>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{
const QString kNavy = QStringLiteral("#111184");
const QString kInk = QStringLiteral("#20233B");
const QString kMuted = QStringLiteral("#777B91");
const QString kLine = QStringLiteral("#E7E9F3");

class ScanProductPage : public QWidget
{
public:
    explicit ScanProductPage(QWidget *parent) : QWidget(parent)
    {
        setStyleSheet(QStringLiteral("background: transparent;"
            "QToolTip { background: #20233B; color: white; border: none; padding: 5px 8px; font-size: 11px; }"));
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 18);
        root->setSpacing(16);

        auto *card = new QFrame(this);
        card->setObjectName(QStringLiteral("scanCard"));
        card->setStyleSheet(QStringLiteral(
            "QFrame#scanCard { background: white; border: 1px solid %1; border-radius: 16px; }").arg(kLine));
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(20, 18, 20, 18);
        layout->setSpacing(10);

        auto *heading = new QHBoxLayout;
        auto *title = new QLabel(QStringLiteral("Look up a product"), card);
        title->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 15px; font-weight: 700; background: transparent;").arg(kInk));
        heading->addWidget(title);
        heading->addStretch(1);
        layout->addLayout(heading);

        auto *hint = new QLabel(QStringLiteral(
            "Point the camera at a product barcode (any angle), or type the 6-digit barcode."), card);
        hint->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; background: transparent;").arg(kMuted));
        layout->addWidget(hint);

        preview_ = new QLabel(QStringLiteral("Camera preview"), card);
        preview_->setAlignment(Qt::AlignCenter);
        preview_->setFixedHeight(220);
        preview_->setStyleSheet(QStringLiteral(
            "background: #15182A; color: white; border-radius: 10px;"));
        preview_->show();
        layout->addWidget(preview_);

        auto *entry = new QHBoxLayout;
        entry->setSpacing(9);
        barcodeEdit_ = new QLineEdit(card);
        barcodeEdit_->setPlaceholderText(QStringLiteral("Enter product barcode"));
        barcodeEdit_->setFixedHeight(40);
        barcodeEdit_->setStyleSheet(QStringLiteral(
            "QLineEdit { background: #FAFAFD; color: %1; border: 1px solid %2; border-radius: 9px;"
            " padding: 0 11px; font-size: 12px; }"
            "QLineEdit:focus { background: white; border: 1px solid %3; }").arg(kInk, kLine, kNavy));
        entry->addWidget(barcodeEdit_, 1);
        auto *lookup = new QPushButton(QStringLiteral("Look up"), card);
        lookup->setCursor(Qt::PointingHandCursor);
        lookup->setFixedSize(96, 40);
        lookup->setStyleSheet(QStringLiteral(
            "QPushButton { background: #F0F0FF; color: %1; border: 1px solid #E1E2FA;"
            " border-radius: 9px; font-size: 11px; font-weight: 700; }"
            "QPushButton:hover { background: #E8E9FF; }").arg(kNavy));
        entry->addWidget(lookup);
        layout->addLayout(entry);

        status_ = new QLabel(card);
        status_->setWordWrap(true);
        status_->hide();
        layout->addWidget(status_);
        root->addWidget(card);

        resultHost_ = new QVBoxLayout;
        root->addLayout(resultHost_);
        root->addStretch(1);

        scanner_ = new CameraBarcodeScanner(this);
        scanner_->setHandlers(
            [this](const QImage &image) {
                if (!preview_->size().isEmpty())
                    preview_->setPixmap(QPixmap::fromImage(image).scaled(
                        preview_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            },
            [this](const QString &barcode) {
                barcodeEdit_->setText(barcode);
                lookUp(barcode);
            },
            [this](const QString &error) {
                stopCamera();
                setStatus(error, true);
            });

        connect(lookup, &QPushButton::clicked, this, [this]() { lookUp(barcodeEdit_->text()); });
        connect(barcodeEdit_, &QLineEdit::returnPressed, this, [this]() { lookUp(barcodeEdit_->text()); });
        startCamera();
    }

    ~ScanProductPage() override
    {
        if (scanner_)
            scanner_->stop();
    }

private:
    void startCamera()
    {
        if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
            return;

        const QCameraDevice camera = QMediaDevices::defaultVideoInput();
        if (camera.isNull())
        {
            setStatus(QStringLiteral("No camera is available on this device."), true);
            return;
        }
        QString error;
        preview_->setText(QStringLiteral("Starting camera..."));
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
        preview_->clear();
        preview_->setText(QStringLiteral("Camera preview"));
    }

    void lookUp(QString barcode)
    {
        clearResult();
        barcode = barcode.trimmed();
        if (barcode.isEmpty())
        {
            setStatus(QStringLiteral("Scan or enter a barcode first."), true);
            return;
        }

        QVector<Pos::Product> products;
        QString error;
        if (!Pos::readProducts(Pos::DataPaths::products(), products, error))
        {
            setStatus(error, true);
            return;
        }
        Pos::Product product;
        if (!Pos::findProductByBarcode(products, barcode, product))
        {
            setStatus(QStringLiteral("No product in Product.txt has barcode %1.").arg(barcode), true);
            return;
        }
        setStatus(QStringLiteral("Barcode %1 found in Product.txt.").arg(barcode));
        result_ = UiPages::createProductDetailsView(product, this);
        resultHost_->addWidget(result_);
    }

    void clearResult()
    {
        if (result_)
        {
            result_->deleteLater();
            result_ = nullptr;
        }
    }

    void setStatus(const QString &message, bool isError = false)
    {
        status_->setText(message);
        status_->setStyleSheet(QStringLiteral("color: %1; font-size: 11px; background: transparent;")
            .arg(isError ? QStringLiteral("#B42318") : QStringLiteral("#18764A")));
        status_->show();
    }

    CameraBarcodeScanner *scanner_ = nullptr;
    QLabel *preview_ = nullptr;
    QLineEdit *barcodeEdit_ = nullptr;
    QLabel *status_ = nullptr;
    QVBoxLayout *resultHost_ = nullptr;
    QWidget *result_ = nullptr;
};
}

QWidget *UiPages::createScanProductPage(QWidget *parent)
{
    return new ScanProductPage(parent);
}
