#include "barcode_image.h"

#include "../../BarCode/common/common.h"
#include "../../BarCode/encoder/encoder.h"
#include "../../BarCode/generator/generator.h"

#include <QDir>
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#include <QRegularExpression>

namespace Pos
{
QString barcodeImageFileName(const QString &barcode, const QString &productName)
{
    QString fileStem = productName;
    fileStem.replace(QLatin1Char('_'), QLatin1Char(' '));
    fileStem = fileStem.trimmed().toLower();
    fileStem.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("_"));
    fileStem.remove(QRegularExpression(QStringLiteral("^_+|_+$")));
    if (fileStem.isEmpty())
        fileStem = QStringLiteral("product");
    return fileStem + QLatin1Char('_') + barcode + QStringLiteral(".png");
}

QImage renderBarcodeImage(const QString &barcode, const QString &productName,
    QString &error)
{
    const std::vector<std::string> patterns = encodeText(barcode.toStdString());
    if (patterns.empty())
    {
        error = QStringLiteral("Barcode %1 cannot be encoded.").arg(barcode);
        return {};
    }

    const Image code = generateBarcode(patterns);
    if (code.width <= 0 || code.height <= 0 ||
        code.pixels.size() != static_cast<size_t>(code.width) * code.height)
    {
        error = QStringLiteral("The barcode generator returned an invalid image.");
        return {};
    }

    QImage bars(code.width, code.height, QImage::Format_Grayscale8);
    for (int y = 0; y < code.height; ++y)
    {
        uchar *row = bars.scanLine(y);
        for (int x = 0; x < code.width; ++x)
            row[x] = static_cast<uchar>(code.pixels[static_cast<size_t>(y) * code.width + x]);
    }
    // Enlarge with hard edges so the bars stay sharp in a print-out.
    bars = bars.scaled(bars.width() * 3, bars.height() * 2,
        Qt::IgnoreAspectRatio, Qt::FastTransformation);

    const int padding = 48;
    const int canvasWidth = bars.width() + padding * 2;
    const int titleHeight = 100;
    const int codeLabelHeight = 76;
    QImage image(canvasWidth, titleHeight + bars.height() + codeLabelHeight,
        QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::TextAntialiasing);
    QFont titleFont = painter.font();
    titleFont.setPixelSize(42);
    titleFont.setWeight(QFont::DemiBold);
    painter.setFont(titleFont);
    const QString displayName = productName.trimmed().replace(QLatin1Char('_'), QLatin1Char(' '));
    const QFontMetrics titleMetrics(titleFont);
    painter.drawText(QRect(padding, 10, bars.width(), titleHeight - 14),
        Qt::AlignCenter, titleMetrics.elidedText(displayName, Qt::ElideRight, bars.width()));
    painter.drawImage(padding, titleHeight, bars);
    QFont barcodeFont = painter.font();
    barcodeFont.setPixelSize(34);
    barcodeFont.setWeight(QFont::DemiBold);
    painter.setFont(barcodeFont);
    painter.drawText(QRect(padding, titleHeight + bars.height(), bars.width(), codeLabelHeight),
        Qt::AlignCenter, barcode);
    painter.end();

    error.clear();
    return image;
}

bool createBarcodeImage(const QString &barcode, const QString &productName,
    const QString &folder, QString &imagePath, QString &error)
{
    imagePath.clear();
    const QImage image = renderBarcodeImage(barcode, productName, error);
    if (image.isNull())
        return false;
    if (!QDir().mkpath(folder))
    {
        error = QStringLiteral("Cannot create the folder %1.").arg(folder);
        return false;
    }
    const QString path = QDir(folder).filePath(barcodeImageFileName(barcode, productName));
    if (!image.save(path, "PNG"))
    {
        error = QStringLiteral("Cannot save the barcode image %1.").arg(path);
        return false;
    }
    imagePath = path;
    error.clear();
    return true;
}
}
