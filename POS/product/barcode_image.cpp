#include "barcode_image.h"

#include "../../BarCode/common/common.h"
#include "../../BarCode/encoder/encoder.h"
#include "../../BarCode/generator/generator.h"

#include <QDir>
#include <QImage>

namespace Pos
{
bool createBarcodeImage(const QString &barcode, const QString &folder,
    QString &imagePath, QString &error)
{
    imagePath.clear();
    const std::vector<std::string> patterns = encodeText(barcode.toStdString());
    if (patterns.empty())
    {
        error = QStringLiteral("Barcode %1 cannot be encoded.").arg(barcode);
        return false;
    }

    const Image code = generateBarcode(patterns);
    if (code.width <= 0 || code.height <= 0 ||
        code.pixels.size() != static_cast<size_t>(code.width) * code.height)
    {
        error = QStringLiteral("The barcode generator returned an invalid image.");
        return false;
    }

    QImage image(code.width, code.height, QImage::Format_Grayscale8);
    for (int y = 0; y < code.height; ++y)
    {
        uchar *row = image.scanLine(y);
        for (int x = 0; x < code.width; ++x)
            row[x] = static_cast<uchar>(code.pixels[static_cast<size_t>(y) * code.width + x]);
    }
    // 3x larger with hard edges (no blur), so the bars stay sharp.
    image = image.scaled(image.width() * 3, image.height() * 2,
        Qt::IgnoreAspectRatio, Qt::FastTransformation);

    if (!QDir().mkpath(folder))
    {
        error = QStringLiteral("Cannot create the folder %1.").arg(folder);
        return false;
    }
    const QString path = QDir(folder).filePath(barcode + QStringLiteral(".png"));
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
