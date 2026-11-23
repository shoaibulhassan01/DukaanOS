#include "product_details_view.h"

#include "../../POS/product/product_service.h"

#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{
const QString kNavy = QStringLiteral("#111184");
const QString kInk = QStringLiteral("#20233B");
const QString kMuted = QStringLiteral("#777B91");
const QString kLine = QStringLiteral("#E7E9F3");

// Image paths in Product.txt are relative to the project folder.
QString resolveImagePath(const QString &path)
{
    if (path.isEmpty() || QFileInfo::exists(path))
        return path;
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../../") + path);
}

QString money(double value)
{
    return QStringLiteral("PKR %1").arg(value, 0, 'f', 2);
}
}

QWidget *UiPages::createProductDetailsView(const Pos::Product &product, QWidget *parent)
{
    auto *view = new QFrame(parent);
    view->setObjectName(QStringLiteral("productDetails"));
    view->setStyleSheet(QStringLiteral(
        "QFrame#productDetails { background: white; border: 1px solid %1; border-radius: 14px; }")
        .arg(kLine));
    auto *layout = new QVBoxLayout(view);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(12);

    QString name = product.name;
    name.replace(QLatin1Char('_'), QLatin1Char(' '));
    auto *title = new QLabel(name.toHtmlEscaped(), view);
    title->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 17px; font-weight: 700; background: transparent;").arg(kInk));
    layout->addWidget(title);

    auto *stockTag = new QLabel(view);
    const bool inStock = product.quantity > 0;
    stockTag->setText(inStock ? QStringLiteral("%1 available").arg(Pos::quantityText(product.quantity, product.unit))
                              : QStringLiteral("Out of stock"));
    stockTag->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 11px; font-weight: 600; background: transparent;")
        .arg(inStock ? QStringLiteral("#18764A") : QStringLiteral("#B42318")));
    layout->addWidget(stockTag);

    auto *image = new QLabel(view);
    image->setAlignment(Qt::AlignCenter);
    image->setMinimumHeight(96);
    image->setStyleSheet(QStringLiteral(
        "background: white; border: 1px dashed %1; border-radius: 10px; color: %2; font-size: 10px;")
        .arg(kLine, kMuted));
    const QPixmap barcode(resolveImagePath(product.barcodeImage));
    if (barcode.isNull())
        image->setText(QStringLiteral("No barcode image found at %1").arg(product.barcodeImage));
    else
        image->setPixmap(barcode.scaledToWidth(360, Qt::SmoothTransformation));
    layout->addWidget(image);

    // The 10 fields of Product.txt, in the required order.
    const QList<QPair<QString, QString>> fields = {
        { QStringLiteral("Product ID"), product.id },
        { QStringLiteral("Barcode"), product.barcode },
        { QStringLiteral("Acronym"), product.acronym },
        { QStringLiteral("Name"), name },
        { QStringLiteral("Measurement unit"), Pos::unitLabel(product.unit) },
        { QStringLiteral("Unit price"), money(product.unitPrice) },
        { QStringLiteral("Tax per unit"), money(product.tax) },
        { QStringLiteral("Discount per unit"), money(product.discountPerUnit) },
        { QStringLiteral("Available quantity"), QString::number(product.quantity) },
        { QStringLiteral("Barcode file"), product.barcodeImage },
    };
    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(24);
    grid->setVerticalSpacing(9);
    for (int i = 0; i < fields.size(); ++i)
    {
        const int row = (i / 2) * 2;
        const int column = i % 2;
        auto *key = new QLabel(fields.at(i).first.toUpper(), view);
        key->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 9px; font-weight: 700; background: transparent;").arg(kMuted));
        auto *value = new QLabel(fields.at(i).second.toHtmlEscaped(), view);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        value->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 12px; font-weight: 600; background: transparent;").arg(kInk));
        grid->addWidget(key, row, column);
        grid->addWidget(value, row + 1, column);
    }
    layout->addLayout(grid);
    return view;
}

void UiPages::showProductDescription(const Pos::Product &product, QWidget *parent)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Product description"));
    dialog.setMinimumWidth(460);
    dialog.setStyleSheet(QStringLiteral("QDialog { background: #F6F7FB; }"));
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(18, 18, 18, 16);
    layout->addWidget(createProductDetailsView(product, &dialog));
    auto *close = new QPushButton(QStringLiteral("Close"), &dialog);
    close->setCursor(Qt::PointingHandCursor);
    close->setFixedHeight(36);
    close->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: white; border: none; border-radius: 8px;"
        " padding: 0 18px; font-size: 11px; font-weight: 700; }"
        "QPushButton:hover { background: #0D0D6B; }").arg(kNavy));
    QObject::connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(close, 0, Qt::AlignRight);
    dialog.exec();
}
