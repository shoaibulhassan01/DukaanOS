
#include "pos_barcode_scanner.h"

#include <QMediaDevices>

namespace Pos
{
    // Owner: Zain Ul Abideen - start scanning with the default camera.
    bool startCameraScan(PosBarcodeScanner& scanner, QString& error)
    {
        if (scanner.isRunning())
        {
            error.clear();
            return true;
        }

        QCameraDevice device = QMediaDevices::defaultVideoInput();

        if (device.isNull())
        {
            error = QStringLiteral("No camera is available on this computer.");
            return false;
        }

        if (!scanner.start(device, error))
        {
            return false;
        }

        error.clear();

        return true;
    }

    // Owner: Zain Ul Abideen - stop scanning when leaving the scan page.
    void stopCameraScan(PosBarcodeScanner& scanner)
    {
        if (scanner.isRunning())
        {
            scanner.stop();
        }
    }
}
