#include "pos_barcode_scanner.h"

namespace Pos
{
// TODO: Start scanning with the computer's default camera.
bool startCameraScan(PosBarcodeScanner &scanner, QString &error)
{
    error = QStringLiteral("TODO: Start camera scanning.");
    return false;
}

// TODO: Stop scanning when the user leaves the scan page.
void stopCameraScan(PosBarcodeScanner &scanner)
{
}
}
