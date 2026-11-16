#include "pos_barcode_scanner.h"

namespace Pos
{
// Owner: Zain Ul Abideen - start scanning with the computer's default camera.
bool startCameraScan(PosBarcodeScanner &scanner, QString &error)
{
    error = QStringLiteral("TODO: Start camera scanning.");
    return false;
}

// Owner: Zain Ul Abideen - stop scanning when the user leaves the scan page.
void stopCameraScan(PosBarcodeScanner &scanner)
{
}
}
