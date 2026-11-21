#pragma once

#include <QString>

namespace Pos
{
namespace DataPaths
{
// Owner: Umar Khan - the one place that knows where the data files live.
// Uses the "Data" folder in the current working folder when it exists
// (run.bat starts the app from the project folder; tests point the working
// folder at a temporary copy), otherwise searches the project's Data folder
// next to or two levels above the .exe.
QString dataDir();

QString users();          // Users.txt
QString products();       // Product.txt
QString orders();         // Order.txt
QString orderDetail();    // OrderDetail.txt
QString bills();          // Bill.txt
QString barcodeFolder();  // barcodes/ (product barcode images)
}
}
