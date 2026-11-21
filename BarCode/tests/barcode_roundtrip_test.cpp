#include "../common/common.h"
#include "../decoder/decoder.h"
#include "../encoder/encoder.h"
#include "../generator/generator.h"
#include "../scanner/scanner.h"

#include <iostream>
#include <QColor>
#include <QImage>

int main()
{
    const std::string digits = "100001";
    const std::vector<std::string> patterns = encodeText(digits);
    if (patterns.empty())
    {
        std::cerr << "Barcode encoding failed.\n";
        return 1;
    }

    const Image barcode = generateBarcode(patterns);
    if (barcode.width <= 0 || barcode.height <= 0)
    {
        std::cerr << "Barcode generation failed.\n";
        return 1;
    }

    QImage image(barcode.width, barcode.height, QImage::Format_Grayscale8);
    for (int y = 0; y < barcode.height; ++y)
    {
        for (int x = 0; x < barcode.width; ++x)
        {
            const int pixel = barcode.pixels[y * barcode.width + x];
            image.setPixelColor(x, y, QColor(pixel, pixel, pixel));
        }
    }

    std::string decoded;
    std::string pattern = imageToString(image);
    std::string error;
    if (pattern.empty() || !decodeRuns(pattern, decoded, error) ||
        decoded != digits)
    {
        std::cerr << "Barcode round-trip failed: " << error << "\n";
        return 1;
    }

    std::cout << "Barcode round-trip test passed.\n";
    return 0;
}
